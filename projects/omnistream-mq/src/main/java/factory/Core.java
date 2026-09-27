package factory;

import java.io.*;
import java.nio.*;
import java.nio.channels.*;
import java.nio.file.*;
import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.*;

public class Core {
    public static final byte MAGIC_BYTE = 0x53; // 'S'
    
    public static final byte CMD_PRODUCE = 0x01;
    public static final byte CMD_CONSUME = 0x02;
    public static final byte CMD_ACK = 0x03;
    public static final byte CMD_NACK = 0x04;
    public static final byte CMD_HEARTBEAT = 0x05;
    public static final byte CMD_RESPONSE = 0x0F;

    public static class WireFrame {
        public final byte command;
        public final int streamId;
        public final byte[] payload;

        public WireFrame(byte command, int streamId, byte[] payload) {
            this.command = command;
            this.streamId = streamId;
            this.payload = payload;
        }

        public byte[] serialize() {
            ByteBuffer buf = ByteBuffer.allocate(1 + 1 + 4 + 4 + payload.length);
            buf.put(MAGIC_BYTE);
            buf.put(command);
            buf.putInt(streamId);
            buf.putInt(payload.length);
            buf.put(payload);
            return buf.array();
        }

        public static WireFrame deserialize(ByteBuffer buf) throws IOException {
            if (buf.remaining() < 10) {
                return null;
            }
            buf.mark();
            byte magic = buf.get();
            if (magic != MAGIC_BYTE) {
                throw new IOException("Invalid magic byte: " + magic);
            }
            byte cmd = buf.get();
            int stId = buf.getInt();
            int len = buf.getInt();
            if (buf.remaining() < len) {
                buf.reset();
                return null;
            }
            byte[] pay = new byte[len];
            buf.get(pay);
            return new WireFrame(cmd, stId, pay);
        }
    }

    public static class Message {
        public final long offset;
        public final long timestamp;
        public final byte[] data;
        public int deliveryAttempts;

        public Message(long offset, long timestamp, byte[] data) {
            this.offset = offset;
            this.timestamp = timestamp;
            this.data = data;
            this.deliveryAttempts = 0;
        }

        public byte[] serialize() {
            ByteBuffer buf = ByteBuffer.allocate(8 + 8 + 4 + 4 + data.length);
            buf.putLong(offset);
            buf.putLong(timestamp);
            buf.putInt(deliveryAttempts);
            buf.putInt(data.length);
            buf.put(data);
            return buf.array();
        }

        public static Message deserialize(ByteBuffer buf) {
            long off = buf.getLong();
            long ts = buf.getLong();
            int attempts = buf.getInt();
            int len = buf.getInt();
            byte[] data = new byte[len];
            buf.get(data);
            Message m = new Message(off, ts, data);
            m.deliveryAttempts = attempts;
            return m;
        }
    }

    public static class PartitionLog {
        private final Path partitionDir;
        private final List<Message> messages = new CopyOnWriteArrayList<>();
        private final AtomicLong nextOffset = new AtomicLong(0);
        private final List<Message> dlq = new CopyOnWriteArrayList<>();
        private final int maxRetries;

        public PartitionLog(Path baseDir, String topic, int partitionId, int maxRetries) throws IOException {
            this.partitionDir = baseDir.resolve(topic).resolve("partition-" + partitionId);
            Files.createDirectories(partitionDir);
            this.maxRetries = maxRetries;
            recoverLog();
        }

        private void recoverLog() throws IOException {
            Path logFile = partitionDir.resolve("00000000000000000000.log");
            if (Files.exists(logFile)) {
                byte[] bytes = Files.readAllBytes(logFile);
                if (bytes.length > 0) {
                    ByteBuffer buf = ByteBuffer.wrap(bytes);
                    while (buf.hasRemaining() && buf.remaining() >= 24) {
                        Message m = Message.deserialize(buf);
                        messages.add(m);
                        nextOffset.set(m.offset + 1);
                    }
                }
            }
        }

        public synchronized long append(byte[] data) throws IOException {
            long offset = nextOffset.getAndIncrement();
            Message m = new Message(offset, System.currentTimeMillis(), data);
            messages.add(m);
            flushToDisk();
            return offset;
        }

        private void flushToDisk() throws IOException {
            Path logFile = partitionDir.resolve("00000000000000000000.log");
            try (FileOutputStream fos = new FileOutputStream(logFile.toFile());
                 FileChannel channel = fos.getChannel()) {
                ByteBuffer combined = ByteBuffer.allocate(messages.size() * 1024);
                for (Message m : messages) {
                    combined.put(m.serialize());
                }
                combined.flip();
                channel.write(combined);
                channel.force(false);
            }
        }

        public List<Message> read(long startOffset, int maxCount) {
            List<Message> result = new ArrayList<>();
            for (Message m : messages) {
                if (m.offset >= startOffset) {
                    result.add(m);
                    if (result.size() >= maxCount) break;
                }
            }
            return result;
        }

        public void handleAck(long offset) {
            messages.removeIf(m -> m.offset == offset);
        }

        public void handleNack(long offset) {
            for (Message m : messages) {
                if (m.offset == offset) {
                    m.deliveryAttempts++;
                    if (m.deliveryAttempts >= maxRetries) {
                        messages.remove(m);
                        dlq.add(m);
                    }
                    break;
                }
            }
        }

        public List<Message> getDlq() {
            return dlq;
        }

        public int size() {
            return messages.size();
        }

        public void prune(long maxAgeMillis) {
            long cutoff = System.currentTimeMillis() - maxAgeMillis;
            messages.removeIf(m -> m.timestamp < cutoff);
        }
    }

    public static class ConsumerGroupCoordinator {
        private final String groupName;
        private final Set<String> members = ConcurrentHashMap.newKeySet();
        private final ConcurrentHashMap<String, Long> heartbeats = new ConcurrentHashMap<>();
        private final List<Integer> partitions = new CopyOnWriteArrayList<>();
        private final ConcurrentHashMap<String, List<Integer>> assignments = new ConcurrentHashMap<>();

        public ConsumerGroupCoordinator(String groupName, List<Integer> partitions) {
            this.groupName = groupName;
            this.partitions.addAll(partitions);
        }

        public synchronized void registerMember(String clientId) {
            members.add(clientId);
            heartbeats.put(clientId, System.currentTimeMillis());
            rebalance();
        }

        public synchronized void heartbeat(String clientId) {
            if (members.contains(clientId)) {
                heartbeats.put(clientId, System.currentTimeMillis());
            }
        }

        public synchronized void removeMember(String clientId) {
            members.remove(clientId);
            heartbeats.remove(clientId);
            assignments.remove(clientId);
            rebalance();
        }

        public synchronized void rebalance() {
            assignments.clear();
            if (members.isEmpty()) return;
            List<String> sortedMembers = new ArrayList<>(members);
            Collections.sort(sortedMembers);
            int pIdx = 0;
            for (Integer p : partitions) {
                String member = sortedMembers.get(pIdx % sortedMembers.size());
                assignments.computeIfAbsent(member, k -> new ArrayList<>()).add(p);
                pIdx++;
            }
        }

        public List<Integer> getAssignment(String clientId) {
            return assignments.getOrDefault(clientId, Collections.emptyList());
        }

        public Set<String> getMembers() {
            return members;
        }
    }

    public static class BrokerServer implements Runnable {
        private final int port;
        private final Path dataDir;
        private volatile boolean running = true;
        private Selector selector;
        private ServerSocketChannel serverChannel;
        private final ConcurrentHashMap<String, PartitionLog> partitionLogs = new ConcurrentHashMap<>();
        private final ConcurrentHashMap<String, ConsumerGroupCoordinator> groups = new ConcurrentHashMap<>();

        public BrokerServer(int port, Path dataDir) {
            this.port = port;
            this.dataDir = dataDir;
        }

        @Override
        public void run() {
            try {
                selector = Selector.open();
                serverChannel = ServerSocketChannel.open();
                serverChannel.configureBlocking(false);
                serverChannel.bind(new java.net.InetSocketAddress(port));
                serverChannel.register(selector, SelectionKey.OP_ACCEPT);

                while (running) {
                    if (selector.select(500) == 0) continue;
                    Iterator<SelectionKey> keys = selector.selectedKeys().iterator();
                    while (keys.hasNext()) {
                        SelectionKey key = keys.next();
                        keys.remove();
                        if (!key.isValid()) continue;
                        if (key.isAcceptable()) {
                            SocketChannel client = serverChannel.accept();
                            client.configureBlocking(false);
                            client.register(selector, SelectionKey.OP_READ, ByteBuffer.allocate(8192));
                        } else if (key.isReadable()) {
                            handleRead(key);
                        }
                    }
                }
            } catch (IOException e) {
                // Server shutdown or network error
            }
        }

        private void handleRead(SelectionKey key) throws IOException {
            SocketChannel channel = (SocketChannel) key.channel();
            ByteBuffer buf = (ByteBuffer) key.attachment();
            int read = channel.read(buf);
            if (read == -1) {
                channel.close();
                key.cancel();
                return;
            }
            buf.flip();
            while (buf.remaining() >= 10) {
                buf.mark();
                try {
                    WireFrame frame = WireFrame.deserialize(buf);
                    if (frame == null) {
                        buf.reset();
                        break;
                    }
                    WireFrame response = processFrame(frame);
                    if (response != null) {
                        channel.write(ByteBuffer.wrap(response.serialize()));
                    }
                } catch (IOException e) {
                    channel.close();
                    key.cancel();
                    return;
                }
            }
            buf.compact();
        }

        public WireFrame processFrame(WireFrame frame) {
            try {
                switch (frame.command) {
                    case CMD_PRODUCE: {
                        // Payload format: [TopicLen(4)][Topic][Partition(4)][DataLen(4)][Data]
                        ByteBuffer bb = ByteBuffer.wrap(frame.payload);
                        int tLen = bb.getInt();
                        byte[] tBytes = new byte[tLen];
                        bb.get(tBytes);
                        String topic = new String(tBytes);
                        int partition = bb.getInt();
                        int dLen = bb.getInt();
                        byte[] data = new byte[dLen];
                        bb.get(data);

                        String key = topic + "-" + partition;
                        PartitionLog pl = partitionLogs.computeIfAbsent(key, k -> {
                            try {
                                return new PartitionLog(dataDir, topic, partition, 3);
                            } catch (IOException e) {
                                throw new RuntimeException(e);
                            }
                        });
                        long off = pl.append(data);
                        ByteBuffer resp = ByteBuffer.allocate(8);
                        resp.putLong(off);
                        return new WireFrame(CMD_RESPONSE, frame.streamId, resp.array());
                    }
                    case CMD_CONSUME: {
                        // Payload format: [TopicLen(4)][Topic][Partition(4)][StartOffset(8)][MaxCount(4)]
                        ByteBuffer bb = ByteBuffer.wrap(frame.payload);
                        int tLen = bb.getInt();
                        byte[] tBytes = new byte[tLen];
                        bb.get(tBytes);
                        String topic = new String(tBytes);
                        int partition = bb.getInt();
                        long startOffset = bb.getLong();
                        int maxCount = bb.getInt();

                        String key = topic + "-" + partition;
                        PartitionLog pl = partitionLogs.get(key);
                        if (pl == null) {
                            return new WireFrame(CMD_RESPONSE, frame.streamId, new byte[0]);
                        }
                        List<Message> msgs = pl.read(startOffset, maxCount);
                        ByteArrayOutputStream baos = new ByteArrayOutputStream();
                        DataOutputStream dos = new DataOutputStream(baos);
                        dos.writeInt(msgs.size());
                        for (Message m : msgs) {
                            byte[] mBytes = m.serialize();
                            dos.writeInt(mBytes.length);
                            dos.write(mBytes);
                        }
                        return new WireFrame(CMD_RESPONSE, frame.streamId, baos.toByteArray());
                    }
                    case CMD_ACK: {
                        // Payload format: [TopicLen(4)][Topic][Partition(4)][Offset(8)]
                        ByteBuffer bb = ByteBuffer.wrap(frame.payload);
                        int tLen = bb.getInt();
                        byte[] tBytes = new byte[tLen];
                        bb.get(tBytes);
                        String topic = new String(tBytes);
                        int partition = bb.getInt();
                        long offset = bb.getLong();

                        String key = topic + "-" + partition;
                        PartitionLog pl = partitionLogs.get(key);
                        if (pl != null) {
                            pl.handleAck(offset);
                        }
                        return new WireFrame(CMD_RESPONSE, frame.streamId, new byte[]{0x01});
                    }
                    case CMD_NACK: {
                        // Payload format: [TopicLen(4)][Topic][Partition(4)][Offset(8)]
                        ByteBuffer bb = ByteBuffer.wrap(frame.payload);
                        int tLen = bb.getInt();
                        byte[] tBytes = new byte[tLen];
                        bb.get(tBytes);
                        String topic = new String(tBytes);
                        int partition = bb.getInt();
                        long offset = bb.getLong();

                        String key = topic + "-" + partition;
                        PartitionLog pl = partitionLogs.get(key);
                        if (pl != null) {
                            pl.handleNack(offset);
                        }
                        return new WireFrame(CMD_RESPONSE, frame.streamId, new byte[]{0x01});
                    }
                    case CMD_HEARTBEAT: {
                        return new WireFrame(CMD_RESPONSE, frame.streamId, new byte[]{0x01});
                    }
                    default:
                        return new WireFrame(CMD_RESPONSE, frame.streamId, new byte[]{0x00});
                }
            } catch (Exception e) {
                return new WireFrame(CMD_RESPONSE, frame.streamId, new byte[]{0x00});
            }
        }

        public void stop() {
            running = false;
            try {
                if (serverChannel != null) serverChannel.close();
                if (selector != null) selector.close();
            } catch (IOException ignored) {}
        }
    }
}
