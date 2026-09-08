package factory;

import java.io.File;
import java.nio.file.Files;
import java.util.List;
import factory.Core.AetherKV;
import factory.Core.Row;
import factory.Core.BloomFilter;
import factory.Core.SQLQuery;

public class CoreTest {

    private static void check(boolean condition, String message) {
        if (!condition) {
            throw new AssertionError("Assertion failed: " + message);
        } else {
            System.out.println("[PASS] " + message);
        }
    }

    public static void main(String[] args) throws Exception {
        File tempDir = Files.createTempDirectory("aetherkv-core-test").toFile();
        tempDir.deleteOnExit();

        // Test 1: Bloom Filter False Positive Rate
        BloomFilter bf = new BloomFilter(100, 0.01);
        bf.add("key1");
        bf.add("key2");
        check(bf.mightContain("key1"), "Bloom Filter should contain key1");
        check(bf.mightContain("key2"), "Bloom Filter should contain key2");
        check(!bf.mightContain("key3"), "Bloom Filter should not contain key3");

        // Test 2: SQL Parser
        SQLQuery insertQuery = Core.parse("INSERT INTO users VALUES ('u1', 'Alice', 30, true)");
        check(insertQuery.type == SQLQuery.Type.INSERT, "Parsed query type should be INSERT");
        check(insertQuery.insertRow.id.equals("u1"), "Parsed row ID should be u1");
        check(insertQuery.insertRow.age == 30, "Parsed row age should be 30");

        boolean parserRejected = false;
        try {
            Core.parse("INSERT INTO users VALUES (invalid)");
        } catch (IllegalArgumentException e) {
            parserRejected = true;
        }
        check(parserRejected, "Parser should reject malformed INSERT query");

        // Test 3: Database Operations
        try (AetherKV db = new AetherKV(tempDir, 1024 * 1024, 16)) {
            db.start();

            db.executeQuery("INSERT INTO users VALUES ('u1', 'Alice', 30, true)");
            db.executeQuery("INSERT INTO users VALUES ('u2', 'Bob', 25, false)");

            List<Row> allUsers = db.executeQuery("SELECT * FROM users");
            check(allUsers.size() == 2, "Should find 2 users in database");

            List<Row> filteredUsers = db.executeQuery("SELECT * FROM users WHERE age > 27");
            check(filteredUsers.size() == 1, "Should find 1 user with age > 27");
            check(filteredUsers.get(0).name.equals("Alice"), "Filtered user should be Alice");

            db.executeQuery("DELETE FROM users WHERE id = 'u1'");
            List<Row> remainingUsers = db.executeQuery("SELECT * FROM users");
            check(remainingUsers.size() == 1, "Should find 1 user after deletion");
            check(remainingUsers.get(0).id.equals("u2"), "Remaining user should be Bob");
        }

        System.out.println("All CoreTest assertions passed successfully!");
    }
}
