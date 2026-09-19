package factory;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;

public class CoreTest {

    private static int assertionsPassed = 0;

    private static void check(boolean condition, String message) {
        if (!condition) {
            throw new AssertionError("Assertion failed: " + message);
        }
        assertionsPassed++;
        System.out.println("[PASS] " + message);
    }

    public static void main(String[] args) {
        System.out.println("Running CoreTest unit assertions...");

        try {
            Path tempDir = Files.createTempDirectory("core_test_dir");
            Path walFile = tempDir.resolve("test.wal");

            // Test 1: LogEntry Initialization
            Core.LogEntry entry = new Core.LogEntry(1L, 100L, "key=value");
            check(entry.getTerm() == 1L, "LogEntry term must be correctly initialized");

            // Test 2: LogEntry Index
            check(entry.getIndex() == 100L, "LogEntry index must be correctly initialized");

            // Test 3: LogEntry Command
            check("key=value".equals(entry.getCommand()), "LogEntry command must be correctly initialized");

            // Test 4: WALManager Appending and Reading
            Core.WALManager wal = new Core.WALManager(walFile);
            wal.append(entry);
            List<Core.LogEntry> entries = wal.readAll();
            check(entries.size() == 1, "WAL must successfully append and read back 1 entry");

            // Test 5: WAL Integrity Verification
            Core.LogEntry readEntry = entries.get(0);
            check(readEntry.getTerm() == 1L, "WAL replayed entry term must match original");

            // Test 6: WAL Index Verification
            check(readEntry.getIndex() == 100L, "WAL replayed entry index must match original");

            // Test 7: WAL Command Verification
            check("key=value".equals(readEntry.getCommand()), "WAL replayed entry command must match original");

            // Test 8: WAL Truncation
            wal.append(new Core.LogEntry(2L, 101L, "key2=value2"));
            check(wal.readAll().size() == 2, "WAL must contain 2 entries before truncation");
            wal.truncate(101L);
            check(wal.readAll().size() == 1, "WAL must contain exactly 1 entry after truncating index 101+");

            // Test 9: PeerInfo Initialization
            Core.PeerInfo peer = new Core.PeerInfo(5, 8080);
            check(peer.getId() == 5, "PeerInfo ID must be correctly set");

            // Test 10: PeerInfo Port
            check(peer.port() == 8080, "PeerInfo port must be correctly set");

            System.out.println("All " + assertionsPassed + " CoreTest assertions passed successfully.");
        } catch (IOException e) {
            throw new AssertionError("IOException during CoreTest execution: " + e.getMessage());
        }
    }
}
