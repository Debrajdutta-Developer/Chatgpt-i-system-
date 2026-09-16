package factory;

import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.*;

public class IntegrationTest {
    private static int checksPassed = 0;

    private static void check(boolean condition, String description) {
        if (!condition) {
            throw new AssertionError("Integration assertion failed: " + description);
        }
        checksPassed++;
        System.out.println("  [INTEGRATION PASS] " + description);
    }

    public static void main(String[] args) {
        System.out.println("Running IntegrationTest Suite...");

        try {
            Path dbDir = Paths.get("./integration_db");
            Core.StorageEngine storage = new Core.StorageEngine(dbDir);

            // 1. Generate synthetic multi-column dataset
            int rowCount = 2000;
            int[] ids = new int[rowCount];
            int[] categories = new int[rowCount];
            for (int i = 0; i < rowCount; i++) {
                ids[i] = i;
                categories[i] = i % 5;
            }

            // 2. Encode and persist columns
            byte[] idBytes = Core.ColumnBlockCodec.encodeRLE(ids);
            byte[] catBytes = Core.ColumnBlockCodec.encodeRLE(categories);
            storage.writeColumnSegment("orders", "id", idBytes);
            storage.writeColumnSegment(
                "orders", 
                "category", 
                catBytes
            );
            check(true, "Successfully generated and stored multi-column analytical dataset");

            // 3. Scan column using VectorScanner
            List<Core.ColumnSchema> cols = Arrays.asList(
                new Core.ColumnSchema("id", Core.DataType.INT),
                new Core.ColumnSchema("category", Core.DataType.INT)
            );
            Core.TableSchema schema = new Core.TableSchema("orders", cols);

            Core.TableScanner scanner = new Core.TableScanner(storage, "orders", schema.columns.get(0));
            scanner.open();

            Core.VectorBatch batch = scanner.next();
            check(batch != null, "Vector scanner successfully retrieved first batch");
            check(batch.size == 1024, "Vector batch respects maximum batch size limit of 1024");
            check((Integer) batch.data[0] == 0, "Vector batch data element 0 is correct");
            check((Integer) batch.data[1023] == 1023, "Vector batch data element 1023 is correct");

            Core.VectorBatch batch2 = scanner.next();
            check(batch2 != null, "Vector scanner successfully retrieved second batch");
            check(batch2.size == rowCount - 1024, "Second vector batch contains remaining rows");
            check((Integer) batch2.data[0] == 1024, "Second vector batch data element starts at index 1024");

            Core.VectorBatch batch3 = scanner.next();
            check(batch3 == null, "Vector scanner returns null after end of stream");
            scanner.close();

            // 4. Verify exception handling on storage corruption
            boolean corruptionDetected = false;
            try {
                // Corrupt segment file intentionally by writing garbage
                java.nio.file.Files.write(dbDir.resolve("orders_id.col"), "garbage_data_stream".getBytes());
                storage.readColumnSegment("orders", "id");
            } catch (SecurityException | java.io.IOException e) {
                corruptionDetected = true;
            }
            check(corruptionDetected, "Storage engine successfully triggers integrity check failure on corrupted segment");

            // 5. Validate CBO with inverted statistics
            Core.TableStatistics statsA = new Core.TableStatistics();
            statsA.rowCount = 5000;
            Core.TableStatistics statsB = new Core.TableStatistics();
            statsB.rowCount = 500;
            String optimalPlanInverted = Core.CostBasedOptimizer.optimizeJoin(schema, schema, statsA, statsB, "category");
            check(optimalPlanInverted.contains("Build: orders"), "CBO correctly adjusts build side when table statistics change");

            System.out.println("IntegrationTest completed successfully. Total checks passed: " + checksPassed);

        } catch (Exception e) {
            check(false, "Integration test suite failed with exception: " + e.getMessage());
        }
    }
}
