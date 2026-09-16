package factory;

import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.*;

public class CoreTest {
    private static int checksPassed = 0;

    private static void check(boolean condition, String description) {
        if (!condition) {
            throw new AssertionError("Assertion failed: " + description);
        }
        checksPassed++;
        System.out.println("  [PASS] " + description);
    }

    public static void main(String[] args) {
        System.out.println("Running CoreTest Suite...");

        // 1. RLE Compression Test
        int[] input = {10, 10, 10, 20, 20, 30, 30, 30, 30};
        byte[] rleBytes = Core.ColumnBlockCodec.encodeRLE(input);
        int[] decodedRLE = Core.ColumnBlockCodec.decodeRLE(rleBytes);
        check(Arrays.equals(input, decodedRLE), "RLE encoding and decoding roundtrip preserves exact values");

        // 2. Empty RLE Test
        byte[] emptyRle = Core.ColumnBlockCodec.encodeRLE(new int[0]);
        int[] emptyDecoded = Core.ColumnBlockCodec.decodeRLE(emptyRle);
        check(emptyDecoded.length == 0, "RLE handles empty array inputs correctly");

        // 3. CRC32 Checksum Integrity Test
        byte[] payload = "ColumnaDB columnar payload data".getBytes();
        byte[] checksum = Core.ColumnBlockCodec.computeCRC32(payload);
        boolean isValid = Core.ColumnBlockCodec.verifyCRC32(payload, checksum);
        check(isValid, "CRC32 checksum verifies untampered byte arrays successfully");

        // 4. CRC32 Corruption Detection Test
        byte[] corruptedPayload = "ColumnaDB columnar payload data modified".getBytes();
        boolean isCorruptValid = Core.ColumnBlockCodec.verifyCRC32(corruptedPayload, checksum);
        check(!isCorruptValid, "CRC32 checksum detects data tampering and corruption");

        // 5. Storage Engine Write and Read Test
        try {
            Path tempDir = Paths.get("./test_db_core");
            Core.StorageEngine engine = new Core.StorageEngine(tempDir);
            engine.writeColumnSegment("users", "age", rleBytes);
            byte[] readBack = engine.readColumnSegment("users", "age");
            check(Arrays.equals(rleBytes, readBack), "Storage engine writes and reads column segments with integrity");
        } catch (Exception e) {
            check(false, "Storage engine IO test failed with exception: " + e.getMessage());
        }

        // 6. SQL Parser Test
        String sql = "SELECT id, name FROM employees WHERE id = 100";
        Core.SimpleQueryParser.QueryAST ast = Core.SimpleQueryParser.parse(sql);
        check(ast.tableName.equalsIgnoreCase("employees"), "SQL Parser correctly extracts table name");
        check(ast.projections.size() == 2, "SQL Parser correctly extracts projection column count");
        check(ast.whereColumn.equalsIgnoreCase("id"), "SQL Parser correctly extracts WHERE filter column");
        check(ast.whereValue == 100, "SQL Parser correctly extracts WHERE filter integer value");

        // 7. Cost-Based Optimizer Join Selection Test
        Core.TableSchema t1 = new Core.TableSchema("small_table", Collections.emptyList());
        Core.TableSchema t2 = new Core.TableSchema("large_table", Collections.emptyList());
        Core.TableStatistics s1 = new Core.TableStatistics();
        s1.rowCount = 100;
        Core.TableStatistics s2 = new Core.TableStatistics();
        s2.rowCount = 100000;
        String plan = Core.CostBasedOptimizer.optimizeJoin(t1, t2, s1, s2, "id");
        check(plan.contains("Build: small_table"), "CBO selects smaller table as build side for Hash Join");

        System.out.println("CoreTest completed successfully. Total assertions passed: " + checksPassed);
    }
}
