package factory;

import java.nio.ByteBuffer;
import java.nio.channels.SocketChannel;
import java.util.*;
import java.util.concurrent.ConcurrentLinkedQueue;

public class Core {

    public static int parseIpAddress(String ipStr) {
        String[] parts = ipStr.split("\\.");
        if (parts.length != 4) {
            throw new IllegalArgumentException("Invalid IPv4 address: " + ipStr);
        }
        int result = 0;
        for (int i = 0; i < 4; i++) {
            int val = Integer.parseInt(parts[i]);
            if (val < 0 || val > 255) {
                throw new IllegalArgumentException("IPv4 octet out of range: " + val);
            }
            result = (result << 8) | (val & 0xFF);
        }
        return result;
    }

    public static String formatIpAddress(int ip) {
        return ((ip >> 24) & 0xFF) + "." +
               ((ip >> 16) & 0xFF) + "." +
               ((ip >> 8) & 0xFF) + "." +
               (ip & 0xFF);
    }

    public static class EthernetFrame {
        public final byte[] destinationMac;
        public final byte[] sourceMac;
        public final int ethertype;
        public final byte[] payload;

        public EthernetFrame(byte[] dest, byte[] src, int ethertype, byte[] payload) {
            this.destinationMac = dest;
            this.sourceMac = src;
            this.ethertype = ethertype;
            this.payload = payload;
        }

        public static EthernetFrame parse(byte[] data) {
            if (data == null || data.length < 14) {
                throw new IllegalArgumentException("Ethernet frame too short: " + (data == null ? 0 : data.length));
            }
            byte[] dest = Arrays.copyOfRange(data, 0, 6);
            byte[] src = Arrays.copyOfRange(data, 6, 12);
            int ethertype = ((data[12] & 0xFF) << 8) | (data[13] & 0xFF);
            byte[] payload = Arrays.copyOfRange(data, 14, data.length);
            return new EthernetFrame(dest, src, ethertype, payload);
        }

        public byte[] serialize() {
            ByteBuffer buf = ByteBuffer.allocate(14 + payload.length);
            buf.put(destinationMac);
            buf.put(sourceMac);
            buf.putShort((short) ethertype);
            buf.put(payload);
            return buf.array();
        }
    }

    public static class IPv4Packet {
        public final int version;
        public final int ihl;
        public final int tos;
        public final int totalLength;
        public final int identification;
        public final int ttl;
        public final int protocol;
        public final int sourceIp;
        public final int destinationIp;
        public final byte[] payload;

        public IPv4Packet(int version, int ihl, int tos, int totalLength, int identification,
                          int ttl, int protocol, int srcIp, int destIp, byte[] payload) {
            this.version = version;
            this.ihl = ihl;
            this.tos = tos;
            this.totalLength = totalLength;
            this.identification = identification;
            this.ttl = ttl;
            this.protocol = protocol;
            this.sourceIp = srcIp;
            this.destinationIp = destIp;
            this.payload = payload;
        }

        public static IPv4Packet parse(byte[] data) {
            if (data == null || data.length < 20) {
                throw new IllegalArgumentException("IPv4 packet too short");
            }
            int vIhl = data[0] & 0xFF;
            int version = vIhl >> 4;
            int ihl = vIhl & 0x0F;
            int headerLen = ihl * 4;
            if (data.length < headerLen) {
                throw new IllegalArgumentException("IPv4 IHL exceeds packet length");
            }
            int tos = data[1] & 0xFF;
            int totalLength = ((data[2] & 0xFF) << 8) | (data[3] & 0xFF);
            int identification = ((data[4] & 0xFF) << 8) | (data[5] & 0xFF);
            int ttl = data[8] & 0xFF;
            int protocol = data[9] & 0xFF;
            int srcIp = ((data[12] & 0xFF) << 24) | ((data[13] & 0xFF) << 16) | ((data[14] & 0xFF) << 8) | (data[15] & 0xFF);
            int destIp = ((data[16] & 0xFF) << 24) | ((data[17] & 0xFF) << 16) | ((data[18] & 0xFF) << 8) | (data[19] & 0xFF);
            byte[] payload = Arrays.copyOfRange(data, headerLen, data.length);
            return new IPv4Packet(version, ihl, tos, totalLength, identification, ttl, protocol, srcIp, destIp, payload);
        }
    }

    public static class TcpSegment {
        public final int sourcePort;
        public final int destinationPort;
        public final long sequenceNumber;
        public final long acknowledgementNumber;
        public final int flags;
        public final int windowSize;
        public final byte[] payload;

        public TcpSegment(int srcPort, int destPort, long seq, long ack, int flags, int windowSize, byte[] payload) {
            this.sourcePort = srcPort;
            this.destinationPort = destPort;
            this.sequenceNumber = seq;
            this.acknowledgementNumber = ack;
            this.flags = flags;
            this.windowSize = windowSize;
            this.payload = payload;
        }

        public static TcpSegment parse(byte[] data) {
            if (data == null || data.length < 20) {
                throw new IllegalArgumentException("TCP segment too short");
            }
            int srcPort = ((data[0] & 0xFF) << 8) | (data[1] & 0xFF);
            int destPort = ((data[2] & 0xFF) << 8) | (data[3] & 0xFF);
            long seq = (((long)(data[4] & 0xFF)) << 24) | (((long)(data[5] & 0xFF)) << 16) | (((long)(data[6] & 0xFF)) << 8) | (data[7] & 0xFF);
            long ack = (((long)(data[8] & 0xFF)) << 24) | (((long)(data[9] & 0xFF)) << 16) | (((long)(data[10] & 0xFF)) << 8) | (data[11] & 0xFF);
            int dataOffset = ((data[12] >> 4) & 0x0F) * 4;
            int flags = data[13] & 0x3F;
            int windowSize = ((data[14] & 0xFF) << 8) | (data[15] & 0xFF);
            byte[] payload = data.length > dataOffset ? Arrays.copyOfRange(data, dataOffset, data.length) : new byte[0];
            return new TcpSegment(srcPort, destPort, seq, ack, flags, windowSize, payload);
        }
    }

    public static class RoutingTable {
        private static class TrieNode {
            TrieNode left;
            TrieNode right;
            String nextHop;
        }

        private final TrieNode root = new TrieNode();

        public void addRoute(String cidr, String nextHop) {
            String[] parts = cidr.split("/");
            int ip = parseIpAddress(parts[0]);
            int prefixLen = Integer.parseInt(parts[1]);
            TrieNode curr = root;
            for (int i = 31; i >= 32 - prefixLen; i--) {
                boolean bit = ((ip >> i) & 1) == 1;
                if (bit) {
                    if (curr.right == null) curr.right = new TrieNode();
                    curr = curr.right;
                } else {
                    if (curr.left == null) curr.left = new TrieNode();
                    curr = curr.left;
                }
            }
            curr.nextHop = nextHop;
        }

        public String lookup(String destIpStr) {
            int ip = parseIpAddress(destIpStr);
            TrieNode curr = root;
            String bestMatch = null;
            if (curr.nextHop != null) bestMatch = curr.nextHop;

            for (int i = 31; i >= 0; i--) {
                boolean bit = ((ip >> i) & 1) == 1;
                if (bit) {
                    if (curr.right != null) {
                        curr = curr.right;
                        if (curr.nextHop != null) bestMatch = curr.nextHop;
                    } else {
                        break;
                    }
                } else {
                    if (curr.left != null) {
                        curr = curr.left;
                        if (curr.nextHop != null) bestMatch = curr.nextHop;
                    } else {
                        break;
                    }
                }
            }
            return bestMatch;
        }
    }

    public static class CongestionController {
        private double cwnd = 1.0;
        private double ssthresh = 16.0;
        private int dupAcks = 0;

        public enum State { SLOW_START, CONGESTION_AVOIDANCE, FAST_RECOVERY }
        private State state = State.SLOW_START;

        public void onAckReceived(boolean isNewAck) {
            if (!isNewAck) {
                dupAcks++;
                if (dupAcks == 3) {
                    ssthresh = Math.max(2.0, cwnd / 2.0);
                    cwnd = ssthresh + 3.0;
                    state = State.FAST_RECOVERY;
                } else if (state == State.FAST_RECOVERY) {
                    cwnd += 1.0;
                }
                return;
            }

            dupAcks = 0;
            if (state == State.FAST_RECOVERY) {
                cwnd = ssthresh;
                state = State.CONGESTION_AVOIDANCE;
                return;
            }

            if (cwnd < ssthresh) {
                cwnd += 1.0;
            } else {
                cwnd += 1.0 / cwnd;
                state = State.CONGESTION_AVOIDANCE;
            }
        }

        public void onTimeout() {
            ssthresh = Math.max(2.0, cwnd / 2.0);
            cwnd = 1.0;
            dupAcks = 0;
            state = State.SLOW_START;
        }

        public double getCwnd() { return cwnd; }
        public double getSsthresh() { return ssthresh; }
        public State getState() { return state; }
    }

    public static class VirtualInterface {
        private final ConcurrentLinkedQueue<byte[]> inbox = new ConcurrentLinkedQueue<>();
        private final ConcurrentLinkedQueue<byte[]> outbox = new ConcurrentLinkedQueue<>();
        private double dropProbability = 0.0;
        private final Random random = new Random();

        public void setDropProbability(double prob) {
            this.dropProbability = prob;
        }

        public void send(byte[] frame) {
            if (random.nextDouble() >= dropProbability) {
                outbox.offer(frame);
            }
        }

        public byte[] pollOutbox() {
            return outbox.poll();
        }

        public void inject(byte[] frame) {
            inbox.offer(frame);
        }

        public byte[] pollInbox() {
            return inbox.poll();
        }
    }

    public static class MetricsCollector {
        private long packetsSent = 0;
        private long packetsDropped = 0;
        private long retransmissions = 0;
        private long bytesTransferred = 0;

        public synchronized void recordSend(long bytes) {
            packetsSent++;
            bytesTransferred += bytes;
        }

        public synchronized void recordDrop() {
            packetsDropped++;
        }

        public synchronized void recordRetransmit() {
            retransmissions++;
        }

        public synchronized long getPacketsSent() { return packetsSent; }
        public synchronized long getPacketsDropped() { return packetsDropped; }
        public synchronized long getRetransmissions() { return retransmissions; }
        public synchronized long getBytesTransferred() { return bytesTransferred; }
    }
}
