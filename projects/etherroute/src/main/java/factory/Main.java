package factory;

import java.util.Arrays;

public class Main {
    public static void main(String[] args) {
        System.out.println("Starting EtherRoute Protocol Engine & Router...");

        // 1. Setup Routing Table & LPM
        Core.RoutingTable rt = new Core.RoutingTable();
        rt.addRoute("10.0.0.0/8", "eth0");
        rt.addRoute("10.1.2.0/24", "eth1");
        rt.addRoute("0.0.0.0/0", "gateway-default");

        String hop1 = rt.lookup("10.1.2.50");
        String hop2 = rt.lookup("192.168.1.1");
        System.out.println("Route lookup for 10.1.2.50 -> " + hop1);
        System.out.println(
            "Route lookup for 192.168.1.1 -> " + hop2
        );

        // 2. Test Packet Parsing
        byte[] sampleFrame = new byte[34];
        Arrays.fill(sampleFrame, (byte) 0xAA);
        // Set Ethertype to IPv4 (0x0800)
        sampleFrame[12] = 0x08;
        sampleFrame[13] = 0x00;
        // Set IPv4 version & IHL (v4, ihl=5)
        sampleFrame[14] = 0x45;
        // Set total length = 20
        sampleFrame[16] = 0x00;
        sampleFrame[17] = 0x14;

        Core.EthernetFrame frame = Core.EthernetFrame.parse(sampleFrame);
        System.out.println("Parsed Ethernet Frame Ethertype: 0x" + Integer.toHexString(frame.ethertype));

        // 3. Test Congestion Controller
        Core.CongestionController cc = new Core.CongestionController();
        System.out.println("Initial CWND: " + cc.getCwnd() + " State: " + cc.getState());
        cc.onAckReceived(true);
        System.out.println("CWND after ACK: " + cc.getCwnd() + " State: " + cc.getState());

        // 4. Test Virtual Interface and Metrics
        Core.VirtualInterface vif = new Core.VirtualInterface();
        Core.MetricsCollector metrics = new Core.MetricsCollector();
        vif.setDropProbability(0.1);
        vif.send(sampleFrame);
        byte[] polled = vif.pollOutbox();
        if (polled != null) {
            metrics.recordSend(polled.length);
        } else {
            metrics.recordDrop();
        }
        System.out.println("Metrics - Sent: " + metrics.getPacketsSent() + " Dropped: " + metrics.getPacketsDropped());
        System.out.println("EtherRoute execution completed successfully.");
    }
}
