package factory;

import java.util.List;

public class IntegrationTest {

    private static void check(boolean condition, String message) {
        if (!condition) {
            throw new AssertionError("INTEGRATION TEST FAILURE: " + message);
        }
        System.out.println("[INTEGRATION PASS] " + message);
    }

    public static void main(String[] args) {
        System.out.println("Running IntegrationTest Suite...");

        // 1. Catalog & Histogram integration
        Core.Catalog catalog = new Core.Catalog();
        catalog.registerTable("orders", List.of("order_id", "customer_id", "amount"));
        Core.ColumnHistogram hist = new Core.ColumnHistogram("amount", 1000);
        hist.addBucket("500", 10);
        catalog.registerHistogram("orders.amount", hist);
        check(catalog.getTableSchema("orders") != null, "assertCatalogTableRegistered");
        check(catalog.getHistogram("orders.amount") != null, "assertCatalogHistogramRegistered");

        // 2. Cost-Based Optimizer integration
        Core.CostBasedOptimizer cbo = new Core.CostBasedOptimizer(catalog);
        Core.SelectStatement stmt = new Core.SelectStatement("orders", List.of("order_id"), "amount", "=", "500");
        String plan = cbo.optimize(stmt);
        check(plan.contains("IndexScan"), "assertCBOProducesIndexScanPlan");

        // 3. Vectorized batch execution integration
        Core.VectorBatch batch = new Core.VectorBatch();
        batch.intData[0] = 100;
        batch.intData[1] = 200;
        batch.size = 2;
        check(batch.size == 2, "assertVectorBatchSizeMatches");
        check(batch.intData[0] == 100, "assertVectorBatchFirstElement");
        check(batch.intData[1] == 200, "assertVectorBatchSecondElement");

        // 4. WAL logging integration
        Core.WriteAheadLog wal = new Core.WriteAheadLog();
        wal.append("BEGIN TRANSACTION 1");
        wal.append("INSERT INTO orders VALUES (1, 10, 500)");
        wal.append("COMMIT 1");
        check(wal.getEntries().size() == 3, "assertWALEntryCountThree");
        check(wal.getEntries().get(0).contains("BEGIN"), "assertWALFirstEntryBegin");

        // 5. Slotted page roundtrip persistence & recovery simulation
        Core.SlottedPage originalPage = new Core.SlottedPage();
        originalPage.insertRecord("persistence_test_payload".getBytes());
        byte[] binaryData = originalPage.getData();

        Core.SlottedPage recoveredPage = new Core.SlottedPage(binaryData);
        check(recoveredPage.getItemCount() == 1, "assertRecoveredPageItemCount");
        String recoveredRec = new String(recoveredPage.getRecord(0));
        check(recoveredRec.equals("persistence_test_payload"), "assertRecoveredPageRecordContent");

        System.out.println("IntegrationTest Suite completed successfully with all assertions verified.");
    }
}
