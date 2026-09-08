package factory;

import java.io.File;
import java.nio.file.Files;
import java.util.List;
import factory.Core.AetherKV;
import factory.Core.Row;

public class IntegrationTest {

    private static void check(boolean condition, String message) {
        if (!condition) {
            throw new AssertionError("Test failed: " + message);
        } else {
            System.out.println("[PASS] " + message);
        }
    }

    public static void main(String[] args) throws Exception {
        File tempDir = Files.createTempDirectory("aetherkv-integration-test").toFile();
        tempDir.deleteOnExit();

        testEndToEndAndRecovery(tempDir);
        testMemoryBudgetAndCompaction(tempDir);
    }

    private static void testEndToEndAndRecovery(File tempDir) throws Exception {
        // 1. Start DB and insert data
        try (AetherKV db = new AetherKV(tempDir, 1024 * 1024, 16)) {
            db.start();
            db.executeQuery("INSERT INTO users VALUES ('u1', 'Alice', 30, true)");
            db.executeQuery("INSERT INTO users VALUES ('u2', 'Bob', 25, false)");

            List<Row> users = db.executeQuery("SELECT * FROM users");
            check(users.size() == 2, "Should find 2 users before crash");
        }
        // DB closed without explicit flush, simulating a sudden crash

        // 2. Reopen DB and verify recovery from WAL
        try (AetherKV db = new AetherKV(tempDir, 1024 * 1024, 16)) {
            db.start();
            List<Row> users = db.executeQuery("SELECT * FROM users");
            check(users.size() == 2, "Should recover 2 users from WAL after crash");

            Row u1 = users.stream().filter(u -> u.id.equals("u1")).findFirst().orElse(null);
            check(u1 != null, "Recovered users should contain u1");
            check(u1.name.equals("Alice"), "u1 name should be Alice");
            check(u1.age == 30, "u1 age should be 30");
            check(u1.active, "u1 active should be true");
        }
    }

    private static void testMemoryBudgetAndCompaction(File tempDir) throws Exception {
        // Use a very small memory budget (100 bytes) to trigger frequent flushes and compactions
        try (AetherKV db = new AetherKV(tempDir, 100, 2)) {
            db.start();

            db.executeQuery("INSERT INTO users VALUES ('u1', 'Alice', 30, true)");
            db.executeQuery("INSERT INTO users VALUES ('u2', 'Bob', 25, false)");
            db.executeQuery("INSERT INTO users VALUES ('u3', 'Charlie', 35, true)");
            db.executeQuery("INSERT INTO users VALUES ('u4', 'Diana', 28, false)");
            db.executeQuery("INSERT INTO users VALUES ('u5', 'Eve', 22, true)");

            // Wait briefly for background Virtual Threads to complete flushes and compactions
            Thread.sleep(500);

            List<Row> users = db.executeQuery("SELECT * FROM users");
            check(users.size() == 5, "Should find all 5 users after background flushes and compactions");

            List<Row> activeUsers = db.executeQuery("SELECT * FROM users WHERE active = true");
            check(activeUsers.size() == 3, "Should find 3 active users");

            List<Row> youngUsers = db.executeQuery("SELECT * FROM users WHERE age < 26");
            check(youngUsers.size() == 2, "Should find 2 users with age < 26");
        }
        System.out.println("All IntegrationTest assertions passed successfully!");
    }
}
