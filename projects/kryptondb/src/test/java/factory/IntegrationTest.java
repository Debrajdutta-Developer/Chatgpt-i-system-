package factory;

import java.io.File;
import java.io.RandomAccessFile;
import java.util.Map;

public class IntegrationTest {
    private static int checksPassed = 0;

    private static void check(boolean condition, String message) {
        if (!condition) {
            throw new AssertionError("Integration assertion failed: " + message);
        }
        checksPassed++;
        System.out.println("INTEGRATION TEST PASSED: " + message);
    }

    public static void main(String[] args) {
        System.out.println("Running KryptonDB Integration Tests...");
        try {
            File tmpDir = new File(System.getProperty("java.io.tmpdir"), "kryptondb_integration_test");
            if (tmpDir.exists()) {
                for (File f : tmpDir.listFiles()) f.delete();
            } else {
                tmpDir.mkdirs();
            }

            // Test 1: Full lifecycle transaction persistence
            Core.Database db1 = new Core.Database(tmpDir);
            long tx1 = db1.beginTransaction();
            db1.put(tx1, "user:1", "{\"name\":\"Alice\",\"role\":\"admin\"}");
            db1.commit(tx1);
            db1.close();
            check(new File(tmpDir, "krypton.wal").exists(), "WAL file persisted on disk after close");

            // Test 2: Crash recovery reloading WAL
            Core.Database db2 = new Core.Database(tmpDir);
            long tx2 = db2.beginTransaction();
            String recoveredUser = db2.get(tx2, "user:1");
            db2.commit(tx2);
            check(recoveredUser != null && recoveredUser.contains("Alice"), "Recovered database retrieves user:1 from WAL");

            // Test 3: Multiple concurrent-like transactions
            long tx3 = db2.beginTransaction();
            db2.put(tx3, "user:2", "{\"name\":\"Bob\",\"role\":\"user\"}");
            db2.commit(tx3);
            long tx4 = db2.beginTransaction();
            check("{\"name\":\"Bob\",\"role\":\"user\"}".equals(db2.get(tx4, "user:2")), "Transaction 4 successfully reads transaction 3 write");
            db2.commit(tx4);

            // Test 4: Rollback isolation verification
            long tx5 = db2.beginTransaction();
            db2.put(tx5, "user:temp", "{\"name\":\"Temp\"}");
            db2.rollback(tx5);
            long tx6 = db2.beginTransaction();
            check(db2.get(tx6, "user:temp") == null, "Rolled back transaction data is completely isolated");
            db2.commit(tx6);

            // Test 5: B+Tree index integration with query engine
            Core.BPlusTreeIndex tree = new Core.BPlusTreeIndex();
            tree.insert("user:1", "{\"name\":\"Alice\"}");
            String indexedDoc = tree.search("user:1");
            Map<String, Object> parsedIndexed = Core.QueryParser.parseDocument(indexedDoc);
            check(Core.QueryParser.evaluate(parsedIndexed, "name", "Alice"), "Indexed document evaluates successfully via query parser");

            // Test 6: Malformed JSON document handling
            Map<String, Object> malformedDoc = Core.QueryParser.parseDocument("INVALID_JSON_STRING");
            check(malformedDoc.isEmpty(), "Malformed JSON document parses into empty map without crashing");

            // Test 7: Simulated disk corruption resilience on WAL
            File walFile = new File(tmpDir, "krypton.wal");
            check(walFile.length() > 0, "WAL file has non-zero length before corruption test");
            try (RandomAccessFile raf = new RandomAccessFile(walFile, "rw")) {
                raf.seek(raf.length() - 1);
                raf.write((byte) 0xFF); // Corrupt last byte
            }
            Core.Database db3 = new Core.Database(tmpDir);
            long tx7 = db3.beginTransaction();
            check(db3.get(tx7, "user:1") != null, "Database successfully recovers despite minor WAL tail corruption");
            db3.commit(tx7);
            db3.close();

            // Test 8: Empty key or value edge cases
            Core.Database db4 = new Core.Database(new File(tmpDir, "sub"));
            long tx8 = db4.beginTransaction();
            db4.put(tx8, "", "{}");
            check("{}".equals(db4.get(tx8, "")), "Database handles empty key storage correctly");
            db4.commit(tx8);
            db4.close();

            System.out.println("All " + checksPassed + " Integration tests passed successfully.");
        } catch (Exception e) {
            System.err.println("INTEGRATION TEST FAILED: " + e.getMessage());
            e.printStackTrace();
            System.exit(1);
        }
    }
}
