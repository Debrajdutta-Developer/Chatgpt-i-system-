package factory;

import java.nio.file.Path;
import java.util.List;

public class Main {
    public static void main(String[] args) {
        System.out.println("Starting DistribuLog Consensus Engine CLI...");
        Path tempDir = Path.of(System.getProperty("java.io.tmpdir"), "distribulog_demo");
        List<Core.PeerInfo> peers = List.of(
            new Core.PeerInfo(1, 9001),
            new Core.PeerInfo(2, 9002),
            new Core.PeerInfo(3, 9003)
        );

        try (Core.NodeServer node = new Core.NodeServer(1, 9001, peers, tempDir)) {
            node.start();
            node.forceLeader();
            System.out.println("Node 1 initialized as LEADER on port 9001.");
            
            boolean success = node.clientWrite("demo_key = demo_value");
            System.out.println("Client write execution status: " + success);
            System.out.println("Retrieved state machine value 'demo_key': " + node.getStateValue("demo_key"));
        } catch (Exception e) {
            System.err.println("Error running DistribuLog node: " + e.getMessage());
        }
    }
}
