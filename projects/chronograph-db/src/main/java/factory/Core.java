package factory;

import java.io.*;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicLong;
import java.util.zip.CRC32;

public class Core {

    public static class VertexRecord {
        private final String id;
        private final long validFrom;
        private final long validTo;
        private final Map<String, String> properties;

        public VertexRecord(String id, long validFrom, long validTo, Map<String, String> properties) {
            this.id = id;
            this.validFrom = validFrom;
            this.validTo = validTo;
            this.properties = new HashMap<>(properties);
        }

        public String getId() { return id; }
        public long getValidFrom() { return validFrom; }
        public long getValidTo() { return validTo; }
        public Map<String, String> getProperties() { return properties; }
    }

    public static class EdgeRecord {
        private final String id;
        private final String sourceId;
        private final String targetId;
        private final long validFrom;
        private final long validTo;
        private final double weight;
        private final Map<String, String> properties;

        public EdgeRecord(String id, String sourceId, String targetId, long validFrom, long validTo, double weight, Map<String, String> properties) {
            this.id = id;
            this.sourceId = sourceId;
            this.targetId = targetId;
            this.validFrom = validFrom;
            this.validTo = validTo;
            this.weight = weight;
            this.properties = new HashMap<>(properties);
        }

        public String getId() { return id; }
        public String getSourceId() { return sourceId; }
        public String getTargetId() { return targetId; }
        public long getValidFrom() { return validFrom; }
        public long getValidTo() { return validTo; }
        public double getWeight() { return weight; }
        public Map<String, String> getProperties() { return properties; }
    }

    public static class ChronoGraphEngine implements AutoCloseable {
        private final Path logPath;
        private final List<VertexRecord> vertices = new ArrayList<>();
        private final List<EdgeRecord> edges = new ArrayList<>();
        private final AtomicLong txSequence = new AtomicLong(0);
        private FileOutputStream fos;
        private BufferedOutputStream bos;

        public ChronoGraphEngine(Path logPath) throws IOException {
            this.logPath = logPath;
            if (Files.exists(logPath)) {
                recover();
            } else {
                Files.createDirectories(logPath.getParent() == null ? Paths.get(".") : logPath.getParent());
                this.fos = new FileOutputStream(logPath.toFile(), true);
                this.bos = new BufferedOutputStream(fos);
            }
        } 

        public synchronized void appendVertex(String id, long validFrom, long validTo, Map<String, String> properties) throws IOException {
            VertexRecord record = new VertexRecord(id, validFrom, validTo, properties);
            vertices.add(record);
            writeLogRecord((byte) 1, serializeVertex(record));
        }

        public synchronized void appendEdge(String id, String sourceId, String targetId, long validFrom, long validTo, double weight, Map<String, String> properties) throws IOException {
            EdgeRecord record = new EdgeRecord(id, sourceId, targetId, validFrom, validTo, weight, properties);
            edges.add(record);
            writeLogRecord((byte) 2, serializeEdge(record));
        }

        public synchronized VertexRecord getVertexAt(String id, long snapshotTime) {
            VertexRecord latestMatch = null;
            for (VertexRecord v : vertices) {
                if (v.getId().equals(id) && snapshotTime >= v.getValidFrom() && snapshotTime < v.getValidTo()) {
                    if (latestMatch == null || v.getValidFrom() > latestMatch.getValidFrom()) {
                        latestMatch = v;
                    }
                }
            }
            return latestMatch;
        }

        public synchronized EdgeRecord getEdgeAt(String id, long snapshotTime) {
            EdgeRecord latestMatch = null;
            for (EdgeRecord e : edges) {
                if (e.getId().equals(id) && snapshotTime >= e.getValidFrom() && snapshotTime < e.getValidTo()) {
                    if (latestMatch == null || e.getValidFrom() > latestMatch.getValidFrom()) {
                        latestMatch = e;
                    }
                }
            }
            return latestMatch;
        }

        public synchronized List<String> findPathBFS(String startId, String targetId, long snapshotTime) {
            VertexRecord startV = getVertexAt(startId, snapshotTime);
            VertexRecord targetV = getVertexAt(targetId, snapshotTime);
            if (startV == null || targetV == null) return Collections.emptyList();

            Queue<List<String>> queue = new LinkedList<>();
            Set<String> visited = new HashSet<>();

            queue.add(Collections.singletonList(startId));
            visited.add(startId);

            while (!queue.isEmpty()) {
                List<String> path = queue.poll();
                String currentId = path.get(path.size() - 1);

                if (currentId.equals(targetId)) {
                    return path;
                }

                for (EdgeRecord e : edges) {
                    if (snapshotTime >= e.getValidFrom() && snapshotTime < e.getValidTo()) {
                        String nextId = null;
                        if (e.getSourceId().equals(currentId)) {
                            nextId = e.getTargetId();
                        } else if (e.getTargetId().equals(currentId)) {
                            nextId = e.getSourceId();
                        }

                        if (nextId != null && !visited.contains(nextId)) {
                            VertexRecord nextV = getVertexAt(nextId, snapshotTime);
                            if (nextV != null) {
                                visited.add(nextId);
                                List<String> newPath = new ArrayList<>(path);
                                newPath.add(nextId);
                                queue.add(newPath);
                            }
                        }
                    }
                }
            }
            return Collections.emptyList();
        }

        public synchronized List<String> findPathDijkstra(String startId, String targetId, long snapshotTime) {
            Map<String, Double> distances = new HashMap<>();
            Map<String, String> predecessors = new HashMap<>();
            PriorityQueue<NodeDistance> pq = new PriorityQueue<>(Comparator.comparingDouble(nd -> nd.distance));

            for (VertexRecord v : vertices) {
                if (snapshotTime >= v.getValidFrom() && snapshotTime < v.getValidTo()) {
                    distances.put(v.getId(), Double.MAX_VALUE);
                }
            }

            if (!distances.containsKey(startId) || !distances.containsKey(targetId)) {
                return Collections.emptyList();
            }

            distances.put(startId, 0.0);
            pq.add(new NodeDistance(startId, 0.0));

            while (!pq.isEmpty()) {
                NodeDistance curr = pq.poll();
                String u = curr.nodeId;

                if (u.equals(targetId)) break;
                if (curr.distance > distances.getOrDefault(u, Double.MAX_VALUE)) continue;

                for (EdgeRecord e : edges) {
                    if (snapshotTime >= e.getValidFrom() && snapshotTime < e.getValidTo()) {
                        String v = null;
                        if (e.getSourceId().equals(u)) v = e.getTargetId();
                        else if (e.getTargetId().equals(u)) v = e.getSourceId();

                        if (v != null && distances.containsKey(v)) {
                            double weight = e.getWeight();
                            double newDist = distances.get(u) + weight;
                            if (newDist < distances.get(v)) {
                                distances.put(v, newDist);
                                predecessors.put(v, u);
                                pq.add(new NodeDistance(v, newDist));
                            }
                        }
                    }
                }
            }

            if (!predecessors.containsKey(targetId) && !startId.equals(targetId)) {
                return Collections.emptyList();
            }

            List<String> path = new LinkedList<>();
            String curr = targetId;
            while (curr != null) {
                path.add(0, curr);
                curr = predecessors.get(curr);
            }
            return path;
        }

        private static class NodeDistance {
            String nodeId;
            double distance;
            NodeDistance(String nodeId, double distance) {
                this.nodeId = nodeId;
                this.distance = distance;
            }
        }

        private void writeLogRecord(byte type, byte[] payload) throws IOException {
            ByteArrayOutputStream baos = new ByteArrayOutputStream();
            DataOutputStream dos = new DataOutputStream(baos);
            dos.writeByte(type);
            dos.writeInt(payload.length);
            dos.write(payload);
            byte[] data = baos.toByteArray();

            CRC32 crc = new CRC32();
            crc.update(data);
            long checksum = crc.getValue();

            DataOutputStream finalDos = new DataOutputStream(bos);
            finalDos.writeLong(checksum);
            finalDos.writeInt(data.length);
            finalDos.write(data);
            bos.flush();
            txSequence.incrementAndGet();
        }

        private byte[] serializeVertex(VertexRecord v) throws IOException {
            ByteArrayOutputStream baos = new ByteArrayOutputStream();
            DataOutputStream dos = new DataOutputStream(baos);
            writeUTF(dos, v.getId());
            dos.writeLong(v.getValidFrom());
            dos.writeLong(v.getValidTo());
            dos.writeInt(v.getProperties().size());
            for (Map.Entry<String, String> entry : v.getProperties().entrySet()) {
                writeUTF(dos, entry.getKey());
                writeUTF(dos, entry.getValue());
            }
            return baos.toByteArray();
        }

        private byte[] serializeEdge(EdgeRecord e) throws IOException {
            ByteArrayOutputStream baos = new ByteArrayOutputStream();
            DataOutputStream dos = new DataOutputStream(baos);
            writeUTF(dos, e.getId());
            writeUTF(dos, e.getSourceId());
            writeUTF(dos, e.getTargetId());
            dos.writeLong(e.getValidFrom());
            dos.writeLong(e.getValidTo());
            dos.writeDouble(e.getWeight());
            dos.writeInt(e.getProperties().size());
            for (Map.Entry<String, String> entry : e.getProperties().entrySet()) {
                writeUTF(dos, entry.getKey());
                writeUTF(dos, entry.getValue());
            }
            return baos.toByteArray();
        }

        private void writeUTF(DataOutputStream dos, String str) throws IOException {
            byte[] bytes = str.getBytes(StandardCharsets.UTF_8);
            dos.writeShort(bytes.length);
            dos.write(bytes);
        }

        private String readUTF(DataInputStream dis) throws IOException {
            int len = dis.readUnsignedShort();
            byte[] bytes = new byte[len];
            dis.readFully(bytes);
            return new String(bytes, StandardCharsets.UTF_8);
        }

        private void recover() throws IOException {
            vertices.clear();
            edges.clear();
            try (FileInputStream fis = new FileInputStream(logPath.toFile());
                 BufferedInputStream bis = new BufferedInputStream(fis);
                 DataInputStream dis = new DataInputStream(bis)) {
                while (true) {
                    long expectedChecksum;
                    int dataLen;
                    try {
                        expectedChecksum = dis.readLong();
                        dataLen = dis.readInt();
                    } catch (EOFException e) {
                        break;
                    }

                    byte[] data = new byte[dataLen];
                    try {
                        dis.readFully(data);
                    } catch (EOFException e) {
                        break; // Truncated tail recovery
                    }

                    CRC32 crc = new CRC32();
                    crc.update(data);
                    if (crc.getValue() != expectedChecksum) {
                        break; // Stop at corruption
                    }

                    DataInputStream payloadDis = new DataInputStream(new ByteArrayInputStream(data));
                    byte type = payloadDis.readByte();
                    int payloadLen = payloadDis.readInt();

                    if (type == 1) {
                        String id = readUTF(payloadDis);
                        long from = payloadDis.readLong();
                        long to = payloadDis.readLong();
                        int propSize = payloadDis.readInt();
                        Map<String, String> props = new HashMap<>();
                        for (int i = 0; i < propSize; i++) {
                            String pk = readUTF(payloadDis);
                            String pv = readUTF(payloadDis);
                            props.put(pk, pv);
                        }
                        vertices.add(new VertexRecord(id, from, to, props));
                    } else if (type == 2) {
                        String id = readUTF(payloadDis);
                        String src = readUTF(payloadDis);
                        String tgt = readUTF(payloadDis);
                        long from = payloadDis.readLong();
                        long to = payloadDis.readLong();
                        double weight = payloadDis.readDouble();
                        int propSize = payloadDis.readInt();
                        Map<String, String> props = new HashMap<>();
                        for (int i = 0; i < propSize; i++) {
                            String pk = readUTF(payloadDis);
                            String pv = readUTF(payloadDis);
                            props.put(pk, pv);
                        }
                        edges.add(new EdgeRecord(id, src, tgt, from, to, weight, props));
                    }
                }
            }
            this.fos = new FileOutputStream(logPath.toFile(), true);
            this.bos = new BufferedOutputStream(fos);
        }

        @Override
        public synchronized void close() throws IOException {
            if (bos != null) {
                bos.flush();
                bos.close();
            }
        }
    }
}
