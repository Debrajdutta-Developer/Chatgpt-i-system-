package factory;

import java.io.*;
import java.nio.*;
import java.nio.channels.FileChannel;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.*;
import java.util.concurrent.*;
import java.util.zip.CRC32;

public class Core {

    public enum DataType { INT, LONG, DOUBLE, STRING }

    public static class ColumnSchema {
        public final String name;
        public final DataType type;
        public ColumnSchema(String name, DataType type) {
            this.name = name;
            this.type = type;
        }
    }

    public static class TableSchema {
        public final String tableName;
        public final List<ColumnSchema> columns;
        public TableSchema(String tableName, List<ColumnSchema> columns) {
            this.tableName = tableName;
            this.columns = columns;
        }
        public int getColumnIndex(String colName) {
            for (int i = 0; i < columns.size(); i++) {
                if (columns.get(i).name.equalsIgnoreCase(colName)) return i;
            }
            return -1;
        }
    }

    public static class TableStatistics {
        public long rowCount;
        public final Map<String, Long> nullCounts = new HashMap<>();
        public final Map<String, Comparable<?>> minValues = new HashMap<>();
        public final Map<String, Comparable<?>> maxValues = new HashMap<>();
        public final Map<String, Integer> distinctCounts = new HashMap<>();
    }

    public static class VectorBatch {
        public final int size;
        public final Object[] data;
        public final boolean[] nulls;
        public VectorBatch(int size, Object[] data, boolean[] nulls) {
            this.size = size;
            this.data = data;
            this.nulls = nulls;
        }
    }

    public interface VectorIterator {
        void open() throws IOException;
        VectorBatch next() throws IOException;
        void close() throws IOException;
    }

    public static class ColumnBlockCodec {
        public static byte[] encodeRLE(int[] values) {
            ByteArrayOutputStream baos = new ByteArrayOutputStream();
            DataOutputStream dos = new DataOutputStream(baos);
            try {
                if (values.length == 0) {
                    dos.writeInt(0);
                    return baos.toByteArray();
                }
                dos.writeInt(values.length);
                int runVal = values[0];
                int runLen = 1;
                for (int i = 1; i < values.length; i++) {
                    if (values[i] == runVal) {
                        runLen++;
                    } else {
                        dos.writeInt(runLen);
                        dos.writeInt(runVal);
                        runVal = values[i];
                        runLen = 1;
                    }
                }
                dos.writeInt(runLen);
                dos.writeInt(runVal);
                dos.flush();
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
            return baos.toByteArray();
        }

        public static int[] decodeRLE(byte[] bytes) {
            DataInputStream dis = new DataInputStream(new ByteArrayInputStream(bytes));
            try {
                int total = dis.readInt();
                if (total == 0) return new int[0];
                int[] result = new int[total];
                int idx = 0;
                while (idx < total && dis.available() > 0) {
                    int len = dis.readInt();
                    int val = dis.readInt();
                    for (int i = 0; i < len && idx < total; i++) {
                        result[idx++] = val;
                    }
                }
                return result;
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
        }

        public static byte[] computeCRC32(byte[] data) {
            CRC32 crc = new CRC32();
            crc.update(data);
            long val = crc.getValue();
            ByteBuffer buf = ByteBuffer.allocate(8);
            buf.putLong(val);
            return buf.array();
        }

        public static boolean verifyCRC32(byte[] data, byte[] expectedChecksum) {
            CRC32 crc = new CRC32();
            crc.update(data);
            long val = crc.getValue();
            ByteBuffer buf = ByteBuffer.allocate(8);
            buf.putLong(val);
            return Arrays.equals(buf.array(), expectedChecksum);
        }
    }

    public static class StorageEngine {
        private final Path dbDir;
        public StorageEngine(Path dbDir) {
            this.dbDir = dbDir;
        }

        public void writeColumnSegment(String tableName, String colName, byte[] payload) throws IOException {
            Files.createDirectories(dbDir);
            Path file = dbDir.resolve(tableName + "_" + colName + ".col");
            byte[] checksum = ColumnBlockCodec.computeCRC32(payload);
            try (FileOutputStream fos = new FileOutputStream(file.toFile())) {
                fos.write(checksum);
                fos.write(payload);
            }
        }

        public byte[] readColumnSegment(String tableName, String colName) throws IOException {
            Path file = dbDir.resolve(tableName + "_" + colName + ".col");
            if (!Files.exists(file)) return new byte[0];
            byte[] allBytes = Files.readAllBytes(file);
            if (allBytes.length < 8) throw new IOException("Corrupted segment file: too small");
            byte[] checksum = Arrays.copyOfRange(allBytes, 0, 8);
            byte[] payload = Arrays.copyOfRange(allBytes, 8, allBytes.length);
            if (!ColumnBlockCodec.verifyCRC32(payload, checksum)) {
                throw new SecurityException("CRC32 checksum mismatch for table " + tableName + " column " + colName);
            }
            return payload;
        }
    }

    public static class CostBasedOptimizer {
        public static String optimizeJoin(TableSchema left, TableSchema right, TableStatistics leftStats, TableStatistics rightStats, String joinCol) {
            long leftRows = leftStats.rowCount;
            long rightRows = rightStats.rowCount;
            if (leftRows <= rightRows) {
                return "HashJoin(Build: " + left.tableName + ", Probe: " + right.tableName + ", On: " + joinCol + ")";
            } else {
                return "HashJoin(Build: " + right.tableName + ", Probe: " + left.tableName + ", On: " + joinCol + ")";
            }
        }
    }

    public static class TableScanner implements VectorIterator {
        private final StorageEngine storage;
        private final String tableName;
        private final ColumnSchema column;
        private int[] decodedValues;
        private int cursor;

        public TableScanner(StorageEngine storage, String tableName, ColumnSchema column) {
            this.storage = storage;
            this.tableName = tableName;
            this.column = column;
        }

        @Override
        public void open() throws IOException {
            byte[] payload = storage.readColumnSegment(tableName, column.name);
            this.decodedValues = ColumnBlockCodec.decodeRLE(payload);
            this.cursor = 0;
        }

        @Override
        public VectorBatch next() throws IOException {
            if (cursor >= decodedValues.length) return null;
            int batchSize = Math.min(1024, decodedValues.length - cursor);
            Object[] data = new Object[batchSize];
            boolean[] nulls = new boolean[batchSize];
            for (int i = 0; i < batchSize; i++) {
                data[i] = decodedValues[cursor + i];
                nulls[i] = false;
            }
            cursor += batchSize;
            return new VectorBatch(batchSize, data, nulls);
        }

        @Override
        public void close() throws IOException {
            cursor = 0;
        }
    }

    public static class SimpleQueryParser {
        public static class QueryAST {
            public String tableName;
            public List<String> projections = new ArrayList<>();
            public String whereColumn;
            public Integer whereValue;
        }

        public static QueryAST parse(String sql) {
            QueryAST ast = new QueryAST();
            String[] parts = sql.trim().split("\\s+");
            boolean selectFound = false, fromFound = false, whereFound = false;
            for (int i = 0; i < parts.length; i++) {
                String p = parts[i];
                if (p.equalsIgnoreCase("SELECT")) {
                    selectFound = true;
                    i++;
                    while (i < parts.length && !parts[i].equalsIgnoreCase("FROM")) {
                        String cleaned = parts[i].replaceAll("\\,", "");
                        ast.projections.add(cleaned);
                        i++;
                    }
                    i--;
                } else if (p.equalsIgnoreCase("FROM")) {
                    if (i + 1 < parts.length) ast.tableName = parts[i + 1];
                } else if (p.equalsIgnoreCase("WHERE")) {
                    if (i + 3 < parts.length) {
                        ast.whereColumn = parts[i + 1];
                        ast.whereValue = Integer.parseInt(parts[i + 3]);
                    }
                }
            }
            return ast;
        }
    }
}
