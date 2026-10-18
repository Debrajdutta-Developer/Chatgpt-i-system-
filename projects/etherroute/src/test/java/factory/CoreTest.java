package factory;

import java.util.Arrays;

public class CoreTest {
    private static int checksPassed = 0;

    private static void check(boolean condition, String description) {
        if (!condition) {
            throw new AssertionError("Assertion failed: " + description);
        }
        checksPassed++;
        System.out.println("[PASS] " + description);
    }

    public static void main(String[] args) {
        System.out.println("Running CoreTest Suite...");

        // Test 1: IP Parsing and Formatting
        int ipInt = Core.parseIpAddress("192.168.1.10");
        check(ipInt != 0, "IP address parses to non-zero integer");
        check(Core.formatIpAddress(ipInt).equals("192.168.1.10"), "IP address formatting roundtrips correctly");

        // Test 2: Ethernet Frame Parsing & Serialization
        byte[] rawFrame = new byte[20];
        Arrays.fill(rawFrame, (byte) 0x11);
        rawFrame[12] = 0x08;
        rawFrame[13] = 0x00;
        Core.EthernetFrame eth = Core.EthernetFrame.parse(rawFrame);
        check(eth.ethertype == 0x0800, "Ethernet frame parses correct ethertype");
        byte[] serialized = eth.serialize();
        check(serialized.length == 20, "Ethernet frame serializes to correct length");

        // Test 3: Routing Table Longest-Prefix Match
        Core.RoutingTable rt = new Core.RoutingTable();
        rt.addRoute("10.0.0.0/8", "gateway-8");
        rt.addRoute("10.1.2.0/24", "gateway-24");
        check(rt.lookup("10.1.2.99").equals("gateway-24"), "LPM selects most specific route /24 over /8");
        check(rt.lookup("10.5.5.5").equals("gateway-8"), "LPM falls back to less specific /8 route");

        // Test 4: Congestion Control State Machine
        Core.CongestionController cc = new Core.CongestionController();
        check(cc.getState() == Core.CongestionController.State.SLOW_START, "Congestion controller starts in SLOW_START");
        cc.onAckReceived(true);
        check(cc.getCwnd() == 2.0, "CWND increases in slow start upon ACK");
        cc.onTimeout();
        check(cc.getCwnd() == 1.0, "CWND resets to 1.0 upon timeout");
        check(cc.getState() == Core.CongestionController.State.SLOW_START, "Congestion controller returns to SLOW_START on timeout");

        // Test 5: Metrics Collector
        Core.MetricsCollector mc = new Core.MetricsCollector();
        mc.recordSend(1500);
        mc.recordDrop();
        check(mc.getPacketsSent() == 1, "Metrics records sent packets correctly");
        check(mc.getPacketsDropped() == 1, "Metrics records dropped packets correctly");

        System.out.println("CoreTest completed successfully. Total checks passed: " + checksPassed);
    }
}
