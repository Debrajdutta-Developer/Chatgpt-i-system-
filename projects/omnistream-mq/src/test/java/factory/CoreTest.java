package factory;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;

public class CoreTest {
    private static int assertionCount = 0;

    private static void check(boolean condition, String message) {
        if (!condition) {
            throw new AssertionError("Assertion failed: " + message);
        }
        assertionCount++;
        System.out.println("[PASS] " + message);
    }

    public static void main(String[] args) throws IOException {
        System.out.println("=== Starting CoreTest ===");

        // Test 1: WireFrame Serialization & Deserialization
        byte[] payload = "Hello OmniStream".getBytes();
        Core.WireFrame original = new Core.WireFrame(Core.CMD_PRODUCE, 42, payload);
        byte[] serialized = original.serialize();
        Core.WireFrame deserialized = Core.WireFrame.deserialize(ByteBuffer.wrap(serialized));
        
        check(deserialized != null, "WireFrame deserializes non-null object");
        check(deserialized.command == Core.CMD_PRODUCE, "WireFrame preserves command type");
        check(deserialized.streamId == 42, "WireFrame preserves stream ID");
        check(java.util.Arrays.equals(deserialized.payload, payload), "WireFrame preserves payload bytes");

        // Test 2: PartitionLog Appends and Persistence
        Path tempDir = Files.createTempDirectory("omni-test-");
        Core.PartitionLog log = new Core.PartitionLog(tempDir, "orders", 0, 3);
        long off1 = log.append("order-1".getBytes());
        long off2 = log.append("order-2".getBytes());

        check(off1 == 0, "First log offset is 0");
        check(off2 == 1, "Second log offset is 1");
        check(log.size() == 2, "PartitionLog contains 2 messages");

        List<Core.Message> readMsgs = log.read(0, 10);
        check(readMsgs.size() == 2, "PartitionLog reads correct message count");
        check(new String(readMsgs.get(0).data).equals("order-1"), "First message data matches");

        // Test 3: Consumer Group Rebalance Coordination
        List<Integer> partitions = List.of(0, 1, 2, 3);
        Core.ConsumerGroupCoordinator group = new Core.ConsumerGroupCoordinator("analytics-group", partitions);
        group.registerMember("client-A");
        group.registerMember("client-B");

        List<Integer> assignA = group.getAssignment("client-A");
        List<Integer> assignB = group.getAssignment("client-B");
        check(assignA.size() == 2, "Consumer group distributes partitions evenly to client-A");
        check(assignB.size() == 2, "Consumer group distributes partitions evenly to client-B");

        // Test 4: Dead-Letter Queue (DLQ) & NACK Handling
        log.handleNack(0);
        log.handleNack(0);
        log.handleNack(0); // Exceeds max retries (3)
        check(log.getDlq().size() == 1, "Message exceeding retry threshold is diverted to DLQ");
        check(new String(log.getDlq().get(0).data).equals("order-1"), "DLQ contains the correct poisoned message");

        // Test 5: ACK removal
        log.handleAck(1);
        check(log.size() == 0, "Acknowledged message is successfully removed from active log");

        System.out.println("=== CoreTest Completed Successfully: " + assertionCount + " assertions verified ===");
    }
}
