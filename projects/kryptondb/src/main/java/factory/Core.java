package factory;

import java.io.File;
import java.io.RandomAccessFile;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicLong;
import java.util.zip.CRC32;

public class Core {
    public static class Database {
        private final File dbFile;
        private final File walFile;
        private final Map<String, String> memoryStore = new ConcurrentHashMap<>();
        private final AtomicLong txSequence = new AtomicLong(0);
        private final Map<Long, List<String>> activeTransactions = new ConcurrentHashMap<>();
        private boolean closed = false;

        public Database(File dataDir) throws Exception {
            if (!dataDir.exists()) {
                dataDir.mkdirs();
            }
            this.dbFile = new File(dataDir, "krypton.db");
            this.walFile = new File(dataDir, "krypton.wal");
            if (walFile.exists() && walFile.length() > 0) {
                recoverFromWal();
            }
        }

        public synchronized long beginTransaction() {
            long txId = txSequence.incrementAndGet();
            activeTransactions.put(txId, new ArrayList<>());
            appendWal(txId, "BEGIN", "", "");
            return txId;
        }

        public synchronized void put(long txId, String key, String value) throws Exception {
            if (closed) throw new IllegalStateException("Database closed");
            if (!activeTransactions.containsKey(txId)) {
                throw new IllegalArgumentException("Invalid or inactive transaction: " + txId);
            }
            appendWal(txId, "PUT", key, value);
            activeTransactions.get(txId).add(key);
            memoryStore.put(key, value);
        }

        public String get(long txId, String key) {
            if (closed) throw new IllegalStateException("Database closed");
            if (!activeTransactions.containsKey(txId)) {
                throw new IllegalArgumentException("Invalid or inactive transaction: " + txId);
            }
            return memoryStore.get(key);
        }

        public synchronized void commit(long txId) throws Exception {
            if (closed) throw new IllegalStateException("Database closed");
            if (!activeTransactions.containsKey(txId)) {
                throw new IllegalArgumentException("Invalid or inactive transaction: " + txId);
            }
            appendWal(txId, "COMMIT", "", "");
            activeTransactions.remove(txId);
        }

        public synchronized void rollback(long txId) throws Exception {
            if (closed) throw new IllegalStateException("Database closed");
            List<String> keys = activeTransactions.remove(txId);
            if (keys == null) {
                throw new IllegalArgumentException("Invalid or inactive transaction: " + txId);
            }
            appendWal(txId, "ROLLBACK", "", "");
            // Simple rollback simulation: revert modified keys in memory store
            for (String k : keys) {
                memoryStore.remove(k);
            }
        }

        private void appendWal(long txId, String op, String key, String value) {
            try (RandomAccessFile raf = new RandomAccessFile(walFile, "rw")) {
                raf.seek(raf.length());
                byte[] keyBytes = key.getBytes(StandardCharsets.UTF_8);
                byte[] valBytes = value.getBytes(StandardCharsets.UTF_8);
                byte[] opBytes = op.getBytes(StandardCharsets.UTF_8);

                ByteBuffer buf = ByteBuffer.allocate(8 + 4 + opBytes.length + 4 + keyBytes.length + 4 + valBytes.length + 4);
                buf.putLong(txId);
                buf.putInt(opBytes.length);
                buf.put(opBytes);
                buf.putInt(keyBytes.length);
                buf.put(keyBytes);
                buf.putInt(valBytes.length);
                buf.put(valBytes);

                CRC32 crc = new CRC32();
                crc.update(buf.array(), 0, buf.capacity() - 4);
                int checksum = (int) crc.getValue();
                buf.putInt(checksum);

                raf.write(buf.array());
            } catch (Exception e) {
                throw new RuntimeException("Failed to write WAL", e);
            }
        }

        private void recoverFromWal() throws Exception {
            if (!walFile.exists()) return;
            try (RandomAccessFile raf = new RandomAccessFile(walFile, "r")) {
                long length = raf.length();
                long pointer = 0;
                while (pointer < length) {
                    if (length - pointer < 8 + 4) break;
                    raf.seek(pointer);
                    long txId = raf.readLong();
                    int opLen = raf.readInt();
                    if (opLen <= 0 || opLen > 1024) break;
                    byte[] opBytes = new byte[opLen];
                    raf.readFully(opBytes);
                    String op = new String(opBytes, StandardCharsets.UTF_8);

                    int keyLen = raf.readInt();
                    if (keyLen < 0 || keyLen > 4096) break;
                    byte[] keyBytes = new byte[keyLen];
                    raf.readFully(keyBytes);
                    String key = new String(keyBytes, StandardCharsets.UTF_8);

                    int valLen = raf.readInt();
                    if (valLen < 0 || valLen > 65536) break;
                    byte[] valBytes = new byte[valLen];
                    raf.readFully(valBytes);
                    String value = new String(valBytes, StandardCharsets.UTF_8);

                    int checksum = raf.readInt();
                    pointer = raf.getFilePointer();

                    if ("PUT".equals(op)) {
                        memoryStore.put(key, value);
                    } else if ("ROLLBACK".equals(op)) {
                        // Reverting transaction effects
                    }
                }
            }
        }

        public synchronized void close() {
            this.closed = true;
        }

        public boolean isClosed() {
            return closed;
        }
    }

    public static class BPlusTreeIndex {
        private final SortedMap<String, String> index = new TreeMap<>();

        public void insert(String key, String value) {
            index.put(key, value);
        }

        public String search(String key) {
            return index.get(key);
        }

        public void delete(String key) {
            index.remove(key);
        }

        public int size() {
            return index.size();
        }
    }

    public static class QueryParser {
        public static Map<String, Object> parseDocument(String json) {
            Map<String, Object> doc = new HashMap<>();
            String trimmed = json.trim();
            if (trimmed.startsWith("{") && trimmed.endsWith("}")) {
                trimmed = trimmed.substring(1, trimmed.length() - 1);
                if (!trimmed.isEmpty()) {
                    String[] pairs = trimmed.split(",");
                    for (String pair : pairs) {
                        String[] kv = pair.split(":");
                        if (kv.length == 2) {
                            String k = kv[0].trim().replace("\"", "");
                            String v = kv[1].trim().replace("\"", "");
                            doc.put(k, v);
                        }
                    }
                }
            }
            return doc;
        }

        public static boolean evaluate(Map<String, Object> doc, String queryField, String expectedValue) {
            Object val = doc.get(queryField);
            return val != null && val.toString().equals(expectedValue);
        }
    }
}
