package factory;

import java.io.*;
import java.nio.channels.FileChannel;
import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicLong;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class Core {

    public static class KeyVersion implements Comparable<KeyVersion> {
        public final String key;
        public final long version;

        public KeyVersion(String key, long version) {
            this.key = key;
            this.version = version;
        }

        @Override
        public int compareTo(KeyVersion o) {
            int cmp = this.key.compareTo(o.key);
            if (cmp != 0) return cmp;
            return Long.compare(o.version, this.version); // Descending version
        }

        @Override
        public boolean equals(Object o) {
            if (this == o) return true;
            if (!(o instanceof KeyVersion)) return false;
            KeyVersion that = (KeyVersion) o;
            return version == that.version && Objects.equals(key, that.key);
        }

        @Override
        public int hashCode() {
            return Objects.hash(key, version);
        }
    }

    public static class Value {
        public final boolean isTombstone;
        public final byte[] data;

        public Value(boolean isTombstone, byte[] data) {
            this.isTombstone = isTombstone;
            this.data = data;
        }
    }

    public static class IndexEntry {
        public final String key;
        public final long version;
        public final long offset;

        public IndexEntry(String key, long version, long offset) {
            this.key = key;
            this.version = version;
            this.offset = offset;
        }
    }

    public static class BloomFilter {
        public final int k;
        public final int m;
        public final long[] bits;

        public BloomFilter(int expectedInsertions, double fpp) {
            this.m = Math.max(64, (int) Math.ceil(-expectedInsertions * Math.log(fpp) / (Math.log(2) * Math.log(2))));
            this.k = Math.max(1, (int) Math.round((double) m / expectedInsertions * Math.log(2)));
            int numLongs = (m + 63) / 64;
            this.bits = new long[numLongs];
        }

        public BloomFilter(int k, int m, long[] bits) {
            this.k = k;
            this.m = m;
            this.bits = bits;
        }

        private int[] getHashes(String key) {
            int[] hashes = new int[k];
            int h1 = key.hashCode();
            int h2 = h1 ^ (h1 >>> 16);
            for (int i = 0; i < k; i++) {
                hashes[i] = Math.abs((h1 + i * h2) % m);
            }
            return hashes;
        }

        public void add(String key) {
            for (int h : getHashes(key)) {
                int wordIndex = h / 64;
                int bitIndex = h % 64;
                bits[wordIndex] |= (1L << bitIndex);
            }
        }

        public boolean mightContain(String key) {
            for (int h : getHashes(key)) {
                int wordIndex = h / 64;
                int bitIndex = h % 64;
                if ((bits[wordIndex] & (1L << bitIndex)) == 0) {
                    return false;
                }
            }
            return true;
        }
    }

    public static class Row {
        public final String id;
        public final String name;
        public final int age;
        public final boolean active;

        public Row(String id, String name, int age, boolean active) {
            this.id = id;
            this.name = name;
            this.age = age;
            this.active = active;
        }

        public byte[] serialize() {
            try {
                ByteArrayOutputStream baos = new ByteArrayOutputStream();
                DataOutputStream dos = new DataOutputStream(baos);
                dos.writeUTF(id);
                dos.writeUTF(name);
                dos.writeInt(age);
                dos.writeBoolean(active);
                return baos.toByteArray();
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
        }

        public static Row deserialize(byte[] data) {
            try {
                ByteArrayInputStream bais = new ByteArrayInputStream(data);
                DataInputStream dis = new DataInputStream(bais);
                String id = dis.readUTF();
                String name = dis.readUTF();
                int age = dis.readInt();
                boolean active = dis.readBoolean();
                return new Row(id, name, age, active);
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        public String toString() {
            return "Row{id='" + id + "', name='" + name + "', age=" + age + ", active=" + active + "}";
        }
    }

    public static class SQLQuery {
        public enum Type { SELECT, INSERT, DELETE }
        public final Type type;
        public final String tableName;
        public final Row insertRow;
        public final List<String> selectColumns;
        public final String whereColumn;
        public final String whereOp;
        public final String whereValue;

        public SQLQuery(Type type, String tableName, Row insertRow, List<String> selectColumns, String whereColumn, String whereOp, String whereValue) {
            this.type = type;
            this.tableName = tableName;
            this.insertRow = insertRow;
            this.selectColumns = selectColumns;
            this.whereColumn = whereColumn;
            this.whereOp = whereOp;
            this.whereValue = whereValue;
        }
    }

    public static SQLQuery parse(String sql) {
        sql = sql.trim();
        String lower = sql.toLowerCase();

        if (lower.startsWith("insert")) {
            Pattern pattern = Pattern.compile("(?i)^insert\\s+into\\s+(\\w+)\\s+values\\s*\\(\\s*'([^']*)'\\s*,\\s*'([^']*)'\\s*,\\s*(\\d+)\\s*,\\s*(true|false)\\s*\\)$");
            Matcher matcher = pattern.matcher(sql);
            if (!matcher.matches()) {
                throw new IllegalArgumentException("Malformed INSERT query");
            }
            String tableName = matcher.group(1);
            String id = matcher.group(2);
            String name = matcher.group(3);
            int age = Integer.parseInt(matcher.group(4));
            boolean active = Boolean.parseBoolean(matcher.group(5));
            return new SQLQuery(SQLQuery.Type.INSERT, tableName, new Row(id, name, age, active), null, null, null, null);
        } else if (lower.startsWith("select")) {
            Pattern pattern = Pattern.compile("(?i)^select\\s+(.+?)\\s+from\\s+(\\w+)(?:\\s+where\\s+(\\w+)\\s*([<>=])\\s*(.+))?$");
            Matcher matcher = pattern.matcher(sql);
            if (!matcher.matches()) {
                throw new IllegalArgumentException("Malformed SELECT query");
            }
            String colsStr = matcher.group(1).trim();
            String tableName = matcher.group(2).trim();
            String whereCol = matcher.group(3);
            String whereOp = matcher.group(4);
            String whereVal = matcher.group(5);

            if (whereVal != null) {
                whereVal = whereVal.trim();
                if (whereVal.startsWith("'") && whereVal.endsWith("'")) {
                    whereVal = whereVal.substring(1, whereVal.length() - 1);
                }
            }

            List<String> cols = new ArrayList<>();
            if (!colsStr.equals("*")) {
                for (String c : colsStr.split(",")) {
                    cols.add(c.trim().toLowerCase());
                }
            }
            return new SQLQuery(SQLQuery.Type.SELECT, tableName, null, cols, whereCol, whereOp, whereVal);
        } else if (lower.startsWith("delete")) {
            Pattern pattern = Pattern.compile("(?i)^delete\\s+from\\s+(\\w+)(?:\\s+where\\s+(\\w+)\\s*([<>=])\\s*(.+))?$");
            Matcher matcher = pattern.matcher(sql);
            if (!matcher.matches()) {
                throw new IllegalArgumentException("Malformed DELETE query");
            }
            String tableName = matcher.group(1).trim();
            String whereCol = matcher.group(2);
            String whereOp = matcher.group(3);
            String whereVal = matcher.group(4);

            if (whereVal != null) {
                whereVal = whereVal.trim();
                if (whereVal.startsWith("'") && whereVal.endsWith("'")) {
                    whereVal = whereVal.substring(1, whereVal.length() - 1);
                }
            }
            return new SQLQuery(SQLQuery.Type.DELETE, tableName, null, null, whereCol, whereOp, whereVal);
        } else {
            throw new IllegalArgumentException("Unsupported SQL statement");
        }
    }

    public static class SSTableWriter {
        public static void write(File file, ConcurrentSkipListMap<KeyVersion, Value> memTable, int sparseInterval) throws IOException {
            try (RandomAccessFile raf = new RandomAccessFile(file, "rw")) { 
                raf.setLength(0);
                List<IndexEntry> indexEntries = new ArrayList<>();
                BloomFilter bf = new BloomFilter(memTable.size() + 1, 0.01);

                long currentOffset = 0;
                int count = 0;

                for (Map.Entry<KeyVersion, Value> entry : memTable.entrySet()) {
                    KeyVersion kv = entry.getKey();
                    Value val = entry.getValue();

                    bf.add(kv.key);

                    if (count % sparseInterval == 0) {
                        indexEntries.add(new IndexEntry(kv.key, kv.version, currentOffset));
                    }

                    raf.writeUTF(kv.key);
                    raf.writeLong(kv.version);
                    raf.writeBoolean(val.isTombstone);
                    if (val.data == null) {
                        raf.writeInt(-1);
                    } else {
                        raf.writeInt(val.data.length);
                        raf.write(val.data);
                    }

                    currentOffset = raf.getFilePointer();
                    count++;
                }

                long indexOffset = raf.getFilePointer();
                raf.writeInt(indexEntries.size());
                for (IndexEntry ie : indexEntries) {
                    raf.writeUTF(ie.key);
                    raf.writeLong(ie.version);
                    raf.writeLong(ie.offset);
                }

                long bloomOffset = raf.getFilePointer();
                raf.writeInt(bf.k);
                raf.writeInt(bf.m);
                raf.writeInt(bf.bits.length);
                for (long b : bf.bits) {
                    raf.writeLong(b);
                }

                raf.writeLong(indexOffset);
                raf.writeLong(bloomOffset);
                raf.writeLong(0xAE74E12345678901L);
            }
        }
    }

    public static class SSTableReader {
        private final File file;
        private final List<IndexEntry> index;
        private final BloomFilter bloomFilter;
        private final long indexOffset;
        private final long bloomOffset;

        public SSTableReader(File file) throws IOException {
            this.file = file;
            try (RandomAccessFile raf = new RandomAccessFile(file, "r")) {
                long len = raf.length();
                if (len < 24) {
                    throw new IOException("File too short to be a valid SSTable");
                }
                raf.seek(len - 24);
                this.indexOffset = raf.readLong();
                this.bloomOffset = raf.readLong();
                long magic = raf.readLong();
                if (magic != 0xAE74E12345678901L) {
                    throw new IOException("Invalid magic number in SSTable footer");
                }

                raf.seek(indexOffset);
                int indexSize = raf.readInt();
                this.index = new ArrayList<>(indexSize);
                for (int i = 0; i < indexSize; i++) {
                    String k = raf.readUTF();
                    long v = raf.readLong();
                    long off = raf.readLong();
                    this.index.add(new IndexEntry(k, v, off));
                }

                raf.seek(bloomOffset);
                int k = raf.readInt();
                int m = raf.readInt();
                int bitsLen = raf.readInt();
                long[] bits = new long[bitsLen];
                for (int i = 0; i < bitsLen; i++) {
                    bits[i] = raf.readLong();
                }
                this.bloomFilter = new BloomFilter(k, m, bits);
            }
        }

        public File getFile() { return file; }

        public Value get(String key, long maxVersion) throws IOException {
            if (!bloomFilter.mightContain(key)) {
                return null;
            }

            int low = 0;
            int high = index.size() - 1;
            int candidateIdx = -1;
            while (low <= high) {
                int mid = (low + high) >>> 1;
                int cmp = index.get(mid).key.compareTo(key);
                if (cmp <= 0) {
                    candidateIdx = mid;
                    low = mid + 1;
                } else {
                    high = mid - 1;
                }
            }

            if (candidateIdx == -1) {
                return null;
            }

            long startOffset = index.get(candidateIdx).offset;
            long endOffset = indexOffset;
            if (candidateIdx + 1 < index.size()) {
                endOffset = index.get(candidateIdx + 1).offset;
            }

            try (RandomAccessFile raf = new RandomAccessFile(file, "r")) {
                raf.seek(startOffset);
                Value bestValue = null;
                long bestVersion = -1;

                while (raf.getFilePointer() < endOffset) {
                    String k = raf.readUTF();
                    long v = raf.readLong();
                    boolean isTombstone = raf.readBoolean();
                    int len = raf.readInt();
                    byte[] data = null;
                    if (len >= 0) {
                        data = new byte[len];
                        raf.readFully(data);
                    }

                    int cmp = k.compareTo(key);
                    if (cmp > 0) {
                        break;
                    }
                    if (cmp == 0) {
                        if (v <= maxVersion) {
                            if (v > bestVersion) {
                                bestVersion = v;
                                bestValue = new Value(isTombstone, data);
                            }
                        }
                    }
                }
                return bestValue;
            }
        }
    }

    public static void compact(List<SSTableReader> readers, File outputFile, int sparseInterval) throws IOException {
        ConcurrentSkipListMap<KeyVersion, Value> merged = new ConcurrentSkipListMap<>();
        for (SSTableReader reader : readers) {
            try (RandomAccessFile raf = new RandomAccessFile(reader.getFile(), "r")) {
                raf.seek(0);
                while (raf.getFilePointer() < reader.indexOffset) {
                    String k = raf.readUTF();
                    long v = raf.readLong();
                    boolean isTombstone = raf.readBoolean();
                    int len = raf.readInt();
                    byte[] data = null;
                    if (len >= 0) {
                        data = new byte[len];
                        raf.readFully(data);
                    }
                    KeyVersion kv = new KeyVersion(k, v);
                    merged.put(kv, new Value(isTombstone, data));
                }
            } catch (FileNotFoundException e) {
                // File might have been cleaned up or deleted, skip
            }
        }
        SSTableWriter.write(outputFile, merged, sparseInterval);
    }

    public static class AetherKV implements AutoCloseable {
        private final File dbDir;
        private final long maxMemTableSize;
        private final int sparseInterval;

        private final AtomicLong globalVersion = new AtomicLong(0);
        private ConcurrentSkipListMap<KeyVersion, Value> activeMemTable;
        private final List<SSTableReader> sstables = new CopyOnWriteArrayList<>();

        private File activeWalFile;
        private FileOutputStream walFos;
        private DataOutputStream walDos;

        public AetherKV(File dbDir, long maxMemTableSize, int sparseInterval) {
            this.dbDir = dbDir;
            this.maxMemTableSize = maxMemTableSize;
            this.sparseInterval = sparseInterval;
        }

        public synchronized void start() throws IOException {
            if (!dbDir.exists()) {
                dbDir.mkdirs();
            }

            File[] sstableFiles = dbDir.listFiles((dir, name) -> name.startsWith("sstable_") && name.endsWith(".db"));
            if (sstableFiles != null) {
                Arrays.sort(sstableFiles, Comparator.comparingInt(f -> {
                    String name = f.getName();
                    return Integer.parseInt(name.substring(8, name.length() - 3));
                }));
                for (File f : sstableFiles) {
                    sstables.add(new SSTableReader(f));
                }
            }

            long maxVer = 0;
            for (SSTableReader reader : sstables) {
                try (RandomAccessFile raf = new RandomAccessFile(reader.getFile(), "r")) {
                    raf.seek(0);
                    while (raf.getFilePointer() < reader.indexOffset) {
                        raf.readUTF();
                        long v = raf.readLong();
                        if (v > maxVer) maxVer = v;
                        raf.readBoolean();
                        int len = raf.readInt();
                        if (len >= 0) {
                            raf.skipBytes(len);
                        }
                    }
                } catch (IOException e) {
                    // Ignore corrupted or missing files
                }
            }

            File[] walFiles = dbDir.listFiles((dir, name) -> name.startsWith("wal_") && name.endsWith(".log"));
            if (walFiles != null && walFiles.length > 0) {
                Arrays.sort(walFiles, Comparator.comparingInt(f -> {
                    String name = f.getName();
                    return Integer.parseInt(name.substring(4, name.length() - 4));
                }));

                ConcurrentSkipListMap<KeyVersion, Value> recoveryMemTable = new ConcurrentSkipListMap<>();
                for (File f : walFiles) {
                    replay(f, recoveryMemTable);
                }

                for (KeyVersion kv : recoveryMemTable.keySet()) {
                    if (kv.version > maxVer) {
                        maxVer = kv.version;
                    }
                }

                if (!recoveryMemTable.isEmpty()) {
                    int nextSSTableSeq = getNextSSTableSeq();
                    File sstableFile = new File(dbDir, "sstable_" + nextSSTableSeq + ".db");
                    SSTableWriter.write(sstableFile, recoveryMemTable, sparseInterval);
                    sstables.add(new SSTableReader(sstableFile));
                }

                for (File f : walFiles) {
                    f.delete();
                }
            }

            globalVersion.set(maxVer);

            int nextWalSeq = getNextWalSeq();
            this.activeWalFile = new File(dbDir, "wal_" + nextWalSeq + ".log");
            this.walFos = new FileOutputStream(activeWalFile, true);
            this.walDos = new DataOutputStream(new BufferedOutputStream(walFos));
            this.activeMemTable = new ConcurrentSkipListMap<>();
        }

        private void replay(File walFile, ConcurrentSkipListMap<KeyVersion, Value> memTable) throws IOException {
            try (DataInputStream dis = new DataInputStream(new BufferedInputStream(new FileInputStream(walFile)))) {
                while (true) {
                    try {
                        String key = dis.readUTF();
                        long version = dis.readLong();
                        boolean isTombstone = dis.readBoolean();
                        int len = dis.readInt();
                        byte[] data = null;
                        if (len >= 0) {
                            data = new byte[len];
                            dis.readFully(data);
                        }
                        memTable.put(new KeyVersion(key, version), new Value(isTombstone, data));
                    } catch (EOFException e) {
                        break;
                    }
                }
            }
        }

        public synchronized void put(String key, byte[] data, boolean isTombstone) throws IOException {
            long version = globalVersion.incrementAndGet();

            walDos.writeUTF(key);
            walDos.writeLong(version);
            walDos.writeBoolean(isTombstone);
            if (data == null) {
                walDos.writeInt(-1);
            } else {
                walDos.writeInt(data.length);
                walDos.write(data);
            }
            walDos.flush();
            walFos.getFD().sync();

            activeMemTable.put(new KeyVersion(key, version), new Value(isTombstone, data));

            long estimatedSize = estimateMemTableSize(activeMemTable);
            if (estimatedSize > maxMemTableSize) {
                triggerFlush();
            }
        }

        private long estimateMemTableSize(ConcurrentSkipListMap<KeyVersion, Value> memTable) {
            long size = 0;
            for (Map.Entry<KeyVersion, Value> entry : memTable.entrySet()) {
                size += entry.getKey().key.length() * 2L + 8L;
                Value val = entry.getValue();
                size += 1L;
                if (val.data != null) {
                    size += val.data.length;
                }
                size += 64L;
            }
            return size;
        }

        private void triggerFlush() throws IOException {
            final ConcurrentSkipListMap<KeyVersion, Value> oldMemTable = activeMemTable;
            final File oldWalFile = activeWalFile;

            walDos.close();
            walFos.close();

            int nextWalSeq = getNextWalSeq();
            this.activeWalFile = new File(dbDir, "wal_" + nextWalSeq + ".log");
            this.walFos = new FileOutputStream(activeWalFile, true);
            this.walDos = new DataOutputStream(new BufferedOutputStream(walFos));
            this.activeMemTable = new ConcurrentSkipListMap<>();

            Thread.ofVirtual().start(() -> {
                try {
                    int nextSSTableSeq = getNextSSTableSeq();
                    File sstableFile = new File(dbDir, "sstable_" + nextSSTableSeq + ".db");
                    SSTableWriter.write(sstableFile, oldMemTable, sparseInterval);
                    SSTableReader reader = new SSTableReader(sstableFile);

                    synchronized (this) {
                        sstables.add(reader);
                    }

                    oldWalFile.delete();
                    checkCompaction();
                } catch (IOException e) {
                    e.printStackTrace();
                }
            });
        }

        private synchronized void checkCompaction() {
            if (sstables.size() >= 4) {
                final List<SSTableReader> toCompact = new ArrayList<>(sstables);

                Thread.ofVirtual().start(() -> {
                    try {
                        int nextSSTableSeq = getNextSSTableSeq();
                        File compactedFile = new File(dbDir, "sstable_" + nextSSTableSeq + ".db");
                        compact(toCompact, compactedFile, sparseInterval);

                        SSTableReader reader = new SSTableReader(compactedFile);
                        synchronized (this) {
                            List<SSTableReader> newSstables = new ArrayList<>(sstables);
                            newSstables.removeAll(toCompact);
                            newSstables.add(reader);
                            sstables.clear();
                            sstables.addAll(newSstables);
                        }

                        for (SSTableReader r : toCompact) {
                            r.getFile().delete();
                        }
                    } catch (IOException e) {
                        e.printStackTrace();
                    }
                });
            }
        }

        public synchronized Value get(String key, long maxVersion) throws IOException {
            KeyVersion searchKey = new KeyVersion(key, maxVersion);
            for (Map.Entry<KeyVersion, Value> entry : activeMemTable.tailMap(searchKey).entrySet()) {
                if (entry.getKey().key.equals(key)) {
                    if (entry.getKey().version <= maxVersion) {
                        return entry.getValue();
                    }
                } else {
                    break;
                }
            }

            for (int i = sstables.size() - 1; i >= 0; i--) {
                SSTableReader reader = sstables.get(i);
                Value val = reader.get(key, maxVersion);
                if (val != null) {
                    return val;
                }
            }

            return null;
        }

        public synchronized List<Row> scanAll(long maxVersion) throws IOException {
            Map<String, Value> latest = new TreeMap<>();

            for (Map.Entry<KeyVersion, Value> entry : activeMemTable.entrySet()) {
                KeyVersion kv = entry.getKey();
                if (kv.version <= maxVersion) {
                    latest.putIfAbsent(kv.key, entry.getValue());
                }
            }

            for (int i = sstables.size() - 1; i >= 0; i--) {
                SSTableReader reader = sstables.get(i);
                try (RandomAccessFile raf = new RandomAccessFile(reader.getFile(), "r")) {
                    raf.seek(0);
                    while (raf.getFilePointer() < reader.indexOffset) {
                        String k = raf.readUTF();
                        long v = raf.readLong();
                        boolean isTombstone = raf.readBoolean();
                        int len = raf.readInt();
                        byte[] data = null;
                        if (len >= 0) {
                            data = new byte[len];
                            raf.readFully(data);
                        }

                        if (v <= maxVersion) {
                            latest.putIfAbsent(k, new Value(isTombstone, data));
                        }
                    }
                } catch (IOException e) {
                    // Skip missing or corrupted files
                }
            }

            List<Row> rows = new ArrayList<>();
            for (Map.Entry<String, Value> entry : latest.entrySet()) {
                Value val = entry.getValue();
                if (!val.isTombstone && val.data != null) {
                    rows.add(Row.deserialize(val.data));
                }
            }
            return rows;
        }

        public synchronized List<Row> executeQuery(String sql) throws IOException {
            SQLQuery query = parse(sql);
            long readVersion = globalVersion.get();

            if (query.type == SQLQuery.Type.INSERT) {
                Row row = query.insertRow;
                put(row.id, row.serialize(), false);
                return List.of(row);
            } else if (query.type == SQLQuery.Type.DELETE) {
                List<Row> matches = scanAll(readVersion);
                List<Row> deleted = new ArrayList<>();
                for (Row row : matches) {
                    if (matchesCondition(row, query.whereColumn, query.whereOp, query.whereValue)) {
                        put(row.id, null, true);
                        deleted.add(row);
                    }
                }
                return deleted; 
            } else if (query.type == SQLQuery.Type.SELECT) {
                List<Row> all = scanAll(readVersion);
                List<Row> results = new ArrayList<>();
                for (Row row : all) {
                    if (matchesCondition(row, query.whereColumn, query.whereOp, query.whereValue)) {
                        results.add(row);
                    }
                }
                return results;
            }
            return List.of();
        }

        private boolean matchesCondition(Row row, String col, String op, String val) {
            if (col == null || op == null || val == null) {
                return true;
            }
            col = col.toLowerCase();
            String rowVal;
            if (col.equals("id")) {
                rowVal = row.id;
            } else if (col.equals("name")) {
                rowVal = row.name;
            } else if (col.equals("age")) {
                rowVal = String.valueOf(row.age);
            } else if (col.equals("active")) {
                rowVal = String.valueOf(row.active);
            } else {
                return false;
            }

            if (op.equals("=")) {
                return rowVal.equalsIgnoreCase(val);
            } else if (op.equals(">")) {
                try {
                    double r = Double.parseDouble(rowVal);
                    double v = Double.parseDouble(val);
                    return r > v;
                } catch (NumberFormatException e) {
                    return rowVal.compareTo(val) > 0;
                }
            } else if (op.equals("<")) {
                try {
                    double r = Double.parseDouble(rowVal);
                    double v = Double.parseDouble(val);
                    return r < v;
                } catch (NumberFormatException e) {
                    return rowVal.compareTo(val) < 0;
                }
            }
            return false;
        }

        private int getNextSSTableSeq() {
            File[] files = dbDir.listFiles((dir, name) -> name.startsWith("sstable_") && name.endsWith(".db"));
            int max = 0;
            if (files != null) {
                for (File f : files) {
                    String name = f.getName();
                    try {
                        int seq = Integer.parseInt(name.substring(8, name.length() - 3));
                        if (seq > max) max = seq;
                    } catch (NumberFormatException e) {}
                }
            }
            return max + 1;
        }

        private int getNextWalSeq() {
            File[] files = dbDir.listFiles((dir, name) -> name.startsWith("wal_") && name.endsWith(".log"));
            int max = 0;
            if (files != null) {
                for (File f : files) {
                    String name = f.getName();
                    try {
                        int seq = Integer.parseInt(name.substring(4, name.length() - 4));
                        if (seq > max) max = seq;
                    } catch (NumberFormatException e) {}
                }
            }
            return max + 1;
        }

        @Override
        public synchronized void close() throws IOException {
            if (walDos != null) {
                walDos.close();
            }
            if (walFos != null) {
                walFos.close();
            }
        }
    }
}
