package factory;

import java.util.Arrays;

public class IntegrationTest {
    private static int checksPassed = 0;

    private static void check(boolean condition, String description) {
        if (!condition) {
            throw new AssertionError("Integration assertion failed: " + description);
        }
        checksPassed++;
        System.out.println("[INTEGRATION PASS] " + description);
    }

    public static void main(String[] args) {
        System.out.println("Running IntegrationTest Suite...");

        // 1. Virtual Interface loopback bridging & drop probability
        Core.VirtualInterface vif = new Core.VirtualInterface();
        vif.setDropProbability(0.0); // 0% drop for deterministic integration
        byte[] testPacket = new byte[30];
        Arrays.fill(testPacket, (byte) 0x55);
        vif.send(testPacket);
        byte[] received = vif.pollOutbox();
        check(received != null, "Virtual interface successfully transmits packet through outbox");
        check(received.length == 30, "Transmitted packet length is preserved across interface");

        // 2. Complex routing table overlap resolution
        Core.RoutingTable rt = new Core.RoutingTable();
        rt.addRoute("0.0.0.0/0", "default-gw");
        rt.addRoute("192.168.0.0/16", "local-lan");
        rt.addRoute(
            "192.168.1.0/24",
            "subnet-1"
        );
        check(rt.lookup("192.168.1.50").equals("subnet-1"), "Overlapping routes correctly resolve to /24 prefix");
        check(rt.lookup("192.168.2.50").equals("local-lan"), "Overlapping routes correctly resolve to /16 prefix when /24 does not match");
        check(rt.lookup("8.8.8.8").equals("default-gw"), "Unmatched destination routes correctly to default gateway 0.0.0.0/0");

        // 3. Congestion control rapid triple duplicate ACK fast recovery
        Core.CongestionController cc = new Core.CongestionController();
        cc.onAckReceived(true); // cwnd = 2
        cc.onAckReceived(true); // cwnd = 3
        cc.onAckReceived(false); // dup ack 1
        cc.onAckReceived(false); // dup ack 2
        cc.onAckReceived(false); // dup ack 3 -> triggers fast recovery
        check(cc.getState() == Core.CongestionController.State.FAST_RECOVERY, "Congestion controller enters FAST_RECOVERY upon 3 duplicate ACKs");
        cc.onAckReceived(true); // new ack exits fast recovery
        check(cc.getState() == Core.CongestionController.State.CONGESTION_AVOIDANCE, "Congestion controller transitions to CONGESTION_AVOIDANCE after new ACK in recovery");

        // 4. Packet parser boundary exception tests
        boolean ethernetExceptionThrown = false;
        try {
            Core.EthernetFrame.parse(new byte[5]); // Too short
        } catch (IllegalArgumentException e) {
            ethernetExceptionThrown = true;
        }
        check(ethernetExceptionThrown, "Ethernet parser correctly rejects malformed frames under 14 bytes");

        boolean ipv4ExceptionThrown = false;
        try {
            Core.IPv4Packet.parse(new byte[10]); // Too short
        } catch (IllegalArgumentException e) {
            ipv4ExceptionThrown = true;
        }
        check(ipv4ExceptionThrown, "IPv4 parser correctly rejects malformed packets under 20 bytes");

        // 5. IP Address string conversion boundary checks
        int parsedIp = Core.parseIpAddress("10.1.2.50");
        check(parsedIp != 0, "IP address parsing succeeds for standard private range");

        System.out.println("IntegrationTest completed successfully. Total checks passed: " + checksPassed);
    }
}
