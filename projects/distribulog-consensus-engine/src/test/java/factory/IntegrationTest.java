package factory;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;

public class IntegrationTest {

    private static int assertionsPassed = 0;

    private static void check(boolean condition, String message) {
        if (!condition) {
            throw new AssertionError("Integration Assertion failed: " + message);
        }
        assertionsPassed++;
        System.out.println("[INTEGRATION PASS] " + message);
    }

    public static void main(String[] args) {
        System.out.println("Running IntegrationTest cluster assertions...");

        try {
            Path dir1 = Files.createTempDirectory("node1_dir");
            Path dir2 = Files.createTempDirectory("node2_dir");
            Path dir3 = Files.createTempDirectory("node3_dir");

            List<Core.PeerInfo> peers = List.of(
                new Core.PeerInfo(1, 19001),
                new Core.PeerInfo(2, 19002),
                new Core.PeerInfo(3, 19003)
            );

            try (
                Core.NodeServer node1 = new Core.NodeServer(1, 19001, peers, dir1);
                Core.NodeServer node2 = new Core.NodeServer(2, 19002, peers, dir2);
                Core.NodeServer node3 = new Core.NodeServer(3, 19003, peers, dir3)
            ) {
                node1.start();
                node2.start();
                node3.start();

                // Test 1: Initial state is Follower
                check(node1.getState() == Core.NodeState.FOLLOWER, "Node 1 starts as FOLLOWER");

                // Test 2: Initial state for node 2 is Follower
                check(node2.getState() == Core.NodeState.FOLLOWER, "Node 2 starts as FOLLOWER");

                // Test 3: Initial state for node 3 is Follower
                check(node3.getState() == Core.NodeState.FOLLOWER, "Node 3 starts as FOLLOWER");

                // Test 4: Force Leader Transition on Node 1
                node1.forceLeader();
                check(node1.getState() == Core.NodeState.LEADER, "Node 1 successfully transitions to LEADER");

                // Test 5: Term increments on leader promotion
                check(node1.getTerm() > 0, "Node 1 term is greater than 0 after promotion");

                // Test 6: Client write execution on leader node
                boolean writeSuccess = node1.clientWrite("system = distribulog");
                check(writeSuccess, "Leader node successfully executes client write");

                // Test 7: State machine updates correctly on leader
                String val = node1.getStateValue("system");
                check("distribulog".equals(val), "State machine reflects committed write for 'system'");

                // Test 8: Additional key-value write
                boolean writeSuccess2 = node1.clientWrite("version = 1.0");
                check(writeSuccess2, "Leader node successfully executes second client write");

                // Test 9: State machine retrieves second key correctly
                String val2 = node1.getStateValue("version");
                check("1.0".equals(val2), "State machine reflects committed write for 'version'");

                // Test 10: Non-existent key lookup returns null
                String missingVal = node1.getStateValue("non_existent");
                check(missingVal == null, "State machine returns null for unwritten keys");

                System.out.println("All " + assertionsPassed + " IntegrationTest assertions passed successfully.");
            }
        } catch (IOException e) {
            throw new AssertionError("IOException during IntegrationTest execution: " + e.getMessage());
        }
    }
}
