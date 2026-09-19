package factory;

import java.io.*;
import java.net.*;
import java.nio.*;
import java.nio.channels.*;
import java.nio.file.*;
import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.*;
import java.util.zip.CRC32;

public class Core {

    public enum NodeState {
        FOLLOWER,
        CANDIDATE,
        LEADER
    }

    public static class LogEntry implements Serializable {
        private static final long serialVersionUID = 1L;
        private final long term;
        private final long index;
        private final String command;

        public LogEntry(long term, long index, String command) {
            this.term = term;
            this.index = index;
            this.command = command;
        }

        public long getTerm() { return term; }
        public long getIndex() { return index; }
        public String getCommand() { return command; }
    }

    public static class PeerInfo {
        private final int id;
        private final int port;

        public PeerInfo(int id, int port) {
            this.id = id;
            this.port = port;
        }

        public int getId() { return id; }
        public int port() { return port; }
    }

    public static class WALManager {
        private final Path walPath;

        public WALManager(Path walPath) {
            this.walPath = walPath;
        }

        public synchronized void append(LogEntry entry) throws IOException {
            try (FileOutputStream fos = new FileOutputStream(walPath.toFile(), true);
                 BufferedOutputStream bos = new BufferedOutputStream(fos);
                 DataOutputStream dos = new DataOutputStream(bos)) {
                byte[] payload = entry.getCommand().getBytes(java.nio.charset.StandardCharsets.UTF_8);
                CRC32 crc = new CRC32();
                crc.update((int) entry.getTerm());
                crc.update((int) entry.getIndex());
                crc.update(payload);
                long checksum = crc.getValue();

                dos.writeByte(0xDB);
                dos.writeLong(checksum);
                dos.writeLong(entry.getTerm());
                dos.writeLong(entry.getIndex());
                dos.writeInt(payload.length);
                dos.write(payload);
                dos.flush();
            }
        }

        public synchronized List<LogEntry> readAll() throws IOException {
            List<LogEntry> entries = new ArrayList<>();
            if (!Files.exists(walPath)) {
                return entries;
            }
            try (FileInputStream fis = new FileInputStream(walPath.toFile());
                 BufferedInputStream bis = new BufferedInputStream(fis);
                 DataInputStream dis = new DataInputStream(bis)) {
                while (dis.available() > 0) {
                    byte magic = dis.readByte();
                    if (magic != (byte) 0xDB) {
                        throw new IOException("Invalid WAL magic byte: " + magic);
                    }
                    long checksum = dis.readLong();
                    long term = dis.readLong();
                    long index = dis.readLong();
                    int len = dis.readInt();
                    byte[] payload = new byte[len];
                    dis.readFully(payload);

                    CRC32 crc = new CRC32();
                    crc.update((int) term);
                    crc.update((int) index);
                    crc.update(payload);
                    if (crc.getValue() != checksum) {
                        throw new IOException("CRC32 mismatch in WAL entry at index " + index);
                    }
                    entries.add(new LogEntry(term, index, new String(payload, java.nio.charset.StandardCharsets.UTF_8)));
                }
            }
            return entries;
        }

        public synchronized void truncate(long fromIndex) throws IOException {
            List<LogEntry> entries = readAll();
            if (Files.exists(walPath)) {
                Files.delete(walPath);
            }
            for (LogEntry e : entries) {
                if (e.getIndex() < fromIndex) {
                    append(e);
                }
            }
        }
    }

    public static class NodeServer implements AutoCloseable {
        private final int nodeId;
        private final int port;
        private final List<PeerInfo> peers;
        private final Path storageDir;
        private final AtomicReference<NodeState> state = new AtomicReference<>(NodeState.FOLLOWER);
        private final AtomicLong currentTerm = new AtomicLong(0);
        private final AtomicInteger votedFor = new AtomicInteger(-1);
        private final List<LogEntry> log = new CopyOnWriteArrayList<>();
        private final ConcurrentHashMap<String, String> stateMachine = new ConcurrentHashMap<>();
        
        private volatile boolean running = true;
        private ServerSocketChannel serverChannel;
        private WALManager walManager;
        private Thread serverThread;
        private ScheduledExecutorService scheduler;

        public NodeServer(int nodeId, int port, List<PeerInfo> peers, Path storageDir) throws IOException {
            this.nodeId = nodeId;
            this.port = port;
            this.peers = peers;
            this.storageDir = storageDir;
            Files.createDirectories(storageDir);
            this.walManager = new WALManager(storageDir.resolve("wal_node_" + nodeId + ".log"));
            
            List<LogEntry> existing = walManager.readAll();
            log.addAll(existing);
            for (LogEntry e : existing) {
                applyCommand(e.getCommand());
            }

            this.scheduler = Executors.newScheduledThreadPool(2);
        }

        public void start() throws IOException {
            serverChannel = ServerSocketChannel.open();
            serverChannel.bind(new InetSocketAddress("127.0.0.1", port));
            serverChannel.configureBlocking(true);

            serverThread = Thread.ofVirtual().start(() -> {
                while (running) {
                    try {
                        SocketChannel client = serverChannel.accept();
                        Thread.ofVirtual().start(() -> handleClient(client));
                    } catch (IOException e) {
                        if (!running) break;
                    }
                }
            });

            startElectionTimer();
        }

        private void startElectionTimer() {
            long timeout = 150 + ThreadLocalRandom.current().nextInt(150);
            scheduler.scheduleAtFixedRate(() -> {
                if (!running) return;
                if (state.get() != NodeState.LEADER) {
                    triggerElection();
                }
            }, timeout, timeout, TimeUnit.MILLISECONDS);
        }

        private synchronized void triggerElection() {
            state.set(NodeState.CANDIDATE);
            currentTerm.incrementAndGet();
            votedFor.set(nodeId);

            int votes = 1;
            long lastTerm = log.isEmpty() ? 0 : log.get(log.size() - 1).getTerm();
            long lastIndex = log.isEmpty() ? 0 : log.get(log.size() - 1).getIndex();

            for (PeerInfo peer : peers) {
                if (peer.getId() == nodeId) continue;
                try (SocketChannel socket = SocketChannel.open(new InetSocketAddress("127.0.0.1", peer.port()))) {
                    DataOutputStream out = new DataOutputStream(socket.socket().getOutputStream());
                    DataInputStream in = new DataInputStream(socket.socket().getInputStream());

                    out.writeUTF("REQUEST_VOTE");
                    out.writeLong(currentTerm.get());
                    out.writeInt(nodeId);
                    out.writeLong(lastIndex);
                    out.writeLong(lastTerm);
                    out.flush();

                    String resp = in.readUTF();
                    if ("VOTE_GRANTED".equals(resp)) {
                        votes++;
                    }
                } catch (IOException ignored) {}
            }

            if (votes > (peers.size() + 1) / 2 && state.get() == NodeState.CANDIDATE) {
                state.set(NodeState.LEADER);
                broadcastHeartbeats();
            }
        }

        private void broadcastHeartbeats() {
            if (state.get() != NodeState.LEADER) return;
            for (PeerInfo peer : peers) {
                if (peer.getId() == nodeId) continue;
                try (SocketChannel socket = SocketChannel.open(new InetSocketAddress("127.0.0.1", peer.port()))) {
                    DataOutputStream out = new DataOutputStream(socket.socket().getOutputStream());
                    DataInputStream in = new DataInputStream(socket.socket().getInputStream());

                    out.writeUTF("APPEND_ENTRIES");
                    out.writeLong(currentTerm.get());
                    out.writeInt(nodeId);
                    out.writeLong(0);
                    out.writeLong(0);
                    out.writeInt(0);
                    out.flush();
                    in.readUTF();
                } catch (IOException ignored) {}
            }
        }

        private void handleClient(SocketChannel client) {
            try (
                InputStream is = client.socket().getInputStream();
                OutputStream os = client.socket().getOutputStream();
                DataInputStream dis = new DataInputStream(is);
                DataOutputStream dos = new DataOutputStream(os)
            ) {
                String type = dis.readUTF();
                if ("REQUEST_VOTE".equals(type)) {
                    long term = dis.readLong();
                    int candidateId = dis.readInt();
                    long lastIndex = dis.readLong();
                    long lastTerm = dis.readLong();

                    if (term > currentTerm.get()) {
                        currentTerm.set(term);
                        state.set(NodeState.FOLLOWER);
                        votedFor.set(-1);
                    }

                    if (term == currentTerm.get() && (votedFor.get() == -1 || votedFor.get() == candidateId)) {
                        votedFor.set(candidateId);
                        dos.writeUTF("VOTE_GRANTED");
                    } else {
                        dos.writeUTF("VOTE_DENIED");
                    }
                    dos.flush();
                } else if ("APPEND_ENTRIES".equals(type)) {
                    long term = dis.readLong();
                    int leaderId = dis.readInt();
                    if (term >= currentTerm.get()) {
                        currentTerm.set(term);
                        state.set(NodeState.FOLLOWER);
                    }
                    dos.writeUTF("ACK");
                    dos.flush();
                } else if ("CLIENT_COMMAND".equals(type)) {
                    String cmd = dis.readUTF();
                    if (state.get() == NodeState.LEADER) {
                        long index = log.size() + 1;
                        LogEntry entry = new LogEntry(currentTerm.get(), index, cmd);
                        walManager.append(entry);
                        log.add(entry);
                        applyCommand(cmd);
                        dos.writeUTF("SUCCESS");
                    } else {
                        dos.writeUTF("NOT_LEADER");
                    }
                    dos.flush();
                }
            } catch (IOException ignored) {}
        }

        private void applyCommand(String cmd) {
            String[] parts = cmd.split("=", 2);
            if (parts.length == 2) {
                stateMachine.put(parts[0].trim(), parts[1].trim());
            }
        }

        public boolean clientWrite(String command) {
            if (state.get() == NodeState.LEADER) {
                long index = log.size() + 1;
                LogEntry entry = new LogEntry(currentTerm.get(), index, command);
                try {
                    walManager.append(entry);
                    log.add(entry);
                    applyCommand(command);
                    return true;
                } catch (IOException e) {
                    return false;
                }
            }
            for (PeerInfo peer : peers) {
                if (peer.getId() == nodeId) continue;
                try (SocketChannel socket = SocketChannel.open(new InetSocketAddress("127.0.0.1", peer.port()))) {
                    DataOutputStream out = new DataOutputStream(socket.socket().getOutputStream());
                    DataInputStream in = new DataInputStream(socket.socket().getInputStream());
                    out.writeUTF("CLIENT_COMMAND");
                    out.writeUTF(command);
                    out.flush();
                    String resp = in.readUTF();
                    if ("SUCCESS".equals(resp)) {
                        return true;
                    }
                } catch (IOException ignored) {}
            }
            return false;
        }

        public String getStateValue(String key) {
            return stateMachine.get(key);
        }

        public NodeState getState() {
            return state.get();
        }

        public long getTerm() {
            return currentTerm.get();
        }

        public void forceLeader() {
            state.set(NodeState.LEADER);
            currentTerm.incrementAndGet();
        }

        @Override
        public void close() throws IOException {
            running = false;
            scheduler.shutdownNow();
            if (serverChannel != null) {
                serverChannel.close();
            }
            if (serverThread != null) {
                serverThread.interrupt();
            }
        }
    }
}
