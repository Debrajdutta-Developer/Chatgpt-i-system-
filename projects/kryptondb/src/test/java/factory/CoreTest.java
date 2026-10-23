package factory;

import java.io.File;
import java.util.Map;

public class CoreTest {
    private static int checksPassed = 0;

    private static void check(boolean condition, String message) {
        if (!condition) {
            throw new AssertionError("Assertion failed: " + message);
        }
        checksPassed++;
        System.out.println("CHECK PASSED: " + message);
    }

    public static void main(String[] args) {
        System.out.println("Running KryptonDB Core Unit Tests...");
        try {
            File tmpDir = new File(System.getProperty("java.io.tmpdir"), "kryptondb_core_test");
            if (tmpDir.exists()) {
                for (File f : tmpDir.listFiles()) f.delete();
            }

            // Test 1: Database initialization
            Core.Database db = new Core.Database(tmpDir);
            check(!db.isClosed(), "Database should be open initially");

            // Test 2: Transaction begin and put
            long tx = db.beginTransaction();
            check(tx > 0, "Transaction ID should be positive");
            db.put(tx, "doc:100", "{\"test\":\"value100\"}");
            check("{\"test\":\"value100\"}".equals(db.get(tx, "doc:100")), "Retrieved value should match put value within active transaction");

            // Test 3: Commit transaction
            db.commit(tx);
            long txRead = db.beginTransaction();
            check("{\"test\":\"value100\"}".equals(db.get(txRead, "doc:100")), "Committed value should persist across transactions");
            db.commit(txRead);

            // Test 4: Rollback transaction
            long txRoll = db.beginTransaction();
            db.put(txRoll, "doc:200", "{\"test\":\"value200\"}");
            db.rollback(txRoll);
            long txVerify = db.beginTransaction();
            check(db.get(txVerify, "doc:200") == null, "Rolled back document should not be present");
            db.commit(txVerify);

            // Test 5: B+Tree Index operations
            Core.BPlusTreeIndex tree = new Core.BPlusTreeIndex();
            tree.insert("key1", "val1");
            tree.insert("key2", "val2");
            check(tree.size() == 2, "B+Tree size should be 2 after insertions");
            check("val1".equals(tree.search("key1")), "B+Tree search should return correct value for key1");
            tree.delete("key1");
            check(tree.size() == 1, "B+Tree size should be 1 after deletion");
            check(tree.search("key1") == null, "Deleted key should not be found in B+Tree");

            // Test 6: Query Parser parsing document
            Map<String, Object> parsedDoc = Core.QueryParser.parseDocument("{\"hello\":\"world\",\"id\":\"42\"}");
            check(parsedDoc.size() == 2, "Parsed document should contain 2 fields");
            check("world".equals(parsedDoc.get("hello")), "Parsed document field hello should equal world");

            // Test 7: Query Parser evaluation
            boolean evalRes = Core.QueryParser.evaluate(parsedDoc, "id", "42");
            check(evalRes, "Query parser should evaluate matching condition as true");
            boolean evalFail = Core.QueryParser.evaluate(parsedDoc, "id", "99");
            check(!evalFail, "Query parser should evaluate non-matching condition as false");

            // Test 8: Database closure
            db.close();
            check(db.isClosed(), "Database should report closed state");

            System.out.println("All " + checksPassed + " Core unit tests passed successfully.");
        } catch (Exception e) {
            System.err.println("CoreTest failed with exception: " + e.getMessage());
            e.printStackTrace();
            System.exit(1);
        }
    }
}
