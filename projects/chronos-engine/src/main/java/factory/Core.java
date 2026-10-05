package factory;

import java.lang.invoke.MethodHandles;
import java.lang.invoke.VarHandle;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicLong;

public class Core {

    public static class Timestamp implements Comparable<Timestamp> {
        private final long physical;
        private final int logical;
        private final long nodeId;

        public Timestamp(long physical, int logical, long nodeId) {
            this.physical = physical;
            this.logical = logical;
            this.nodeId = nodeId;
        }

        public long getPhysical() { return physical; }
        public int getLogical() { return logical; }
        public long getNodeId() { return nodeId; }

        @Override
        public int compareTo(Timestamp o) {
            if (this.physical != o.physical) {
                return Long.compare(this.physical, o.physical);
            }
            if (this.logical != o.logical) {
                return Integer.compare(this.logical, o.logical);
            }
            return Long.compare(this.nodeId, o.nodeId);
        }

        @Override
        public boolean equals(Object obj) {
            if (!(obj instanceof Timestamp)) return false;
            Timestamp other = (Timestamp) obj;
            return this.physical == other.physical && this.logical == other.logical && this.nodeId == other.nodeId;
        }

        @Override
        public int hashCode() {
            return Objects.hash(physical, logical, nodeId);
        }

        @Override
        public String toString() {
            return String.format("Timestamp{pt=%d, l=%d, node=%d}", physical, logical, nodeId);
        }
    }

    public static class HybridLogicalClock {
        private final long nodeId;
        private volatile long state;
        private final ClockDriftDetector driftDetector;
        private final AtomicLong overflowCount = new AtomicLong(0);

        private static final VarHandle STATE_HANDLE;
        static {
            try {
                STATE_HANDLE = MethodHandles.lookup().findVarHandle(HybridLogicalClock.class, "state", long.class);
            } catch (ReflectiveOperationException e) {
                throw new ExceptionInInitializerError(e);
            }
        }

        public HybridLogicalClock(long nodeId, ClockDriftDetector driftDetector) {
            this.nodeId = nodeId;
            this.driftDetector = driftDetector;
            this.state = packState(System.currentTimeMillis(), 0);
        }

        private static long packState(long physical, int logical) {
            return (physical << 16) | (logical & 0xFFFFL);
        }

        private static long extractPhysical(long st) {
            return st >>> 16;
        }

        private static int extractLogical(long st) {
            return (int) (st & 0xFFFFL);
        }

        public synchronized Timestamp tick() {
            long currentPhysical = driftDetector.compensate(System.currentTimeMillis());
            long currentState = (long) STATE_HANDLE.get(this);
            long l = extractPhysical(currentState);
            int c = extractLogical(currentState);

            long newPhysical = Math.max(currentPhysical, l);
            int newLogical;

            if (newPhysical == l) {
                if (c == 0xFFFF) {
                    overflowCount.incrementAndGet();
                    newPhysical++;
                    newLogical = 0;
                } else {
                    newLogical = c + 1;
                }
            } else {
                newLogical = 0;
            }

            long newState = packState(newPhysical, newLogical);
            STATE_HANDLE.setVolatile(this, newState);
            return new Timestamp(newPhysical, newLogical, nodeId);
        }

        public synchronized Timestamp update(Timestamp remote) {
            long currentPhysical = driftDetector.compensate(System.currentTimeMillis());
            long currentState = (long) STATE_HANDLE.get(this);
            long l = extractPhysical(currentState);
            int c = extractLogical(currentState);

            long remotePhysical = remote.getPhysical();
            int remoteLogical = remote.getLogical();

            long maxPt = Math.max(Math.max(currentPhysical, l), remotePhysical);
            int newLogical;

            if (maxPt == l && maxPt == remotePhysical) {
                newLogical = Math.max(c, remoteLogical) + 1;
            } else if (maxPt == l) {
                newLogical = c + 1;
            } else if (maxPt == remotePhysical) {
                newLogical = remoteLogical + 1;
            } else {
                newLogical = 0;
            }

            if (newLogical > 0xFFFF) {
                overflowCount.incrementAndGet();
                maxPt++;
                newLogical = 0;
            }

            long newState = packState(maxPt, newLogical);
            STATE_HANDLE.setVolatile(this, newState);
            return new Timestamp(maxPt, newLogical, nodeId);
        }

        public Timestamp getCurrentTime() {
            long currentState = (long) STATE_HANDLE.get(this);
            return new Timestamp(extractPhysical(currentState), extractLogical(currentState), nodeId);
        }

        public long getOverflowCount() {
            return overflowCount.get();
        }
    }

    public static class ClockDriftDetector {
        private final long maxAllowedDriftMs;
        private volatile long lastPhysical = 0;
        private volatile long driftCorrections = 0;

        public ClockDriftDetector(long maxAllowedDriftMs) {
            this.maxAllowedDriftMs = maxAllowedDriftMs;
        }

        public synchronized long compensate(long rawPhysical) {
            if (lastPhysical == 0) {
                lastPhysical = rawPhysical;
                return rawPhysical;
            }

            if (rawPhysical < lastPhysical) {
                long backwardDelta = lastPhysical - rawPhysical;
                if (backwardDelta > maxAllowedDriftMs) {
                    driftCorrections++;
                }
                return lastPhysical;
            }

            if (rawPhysical - lastPhysical > maxAllowedDriftMs) {
                driftCorrections++;
                lastPhysical = lastPhysical + maxAllowedDriftMs;
                return lastPhysical;
            }

            lastPhysical = rawPhysical;
            return rawPhysical;
        }

        public long getDriftCorrections() {
            return driftCorrections;
        }
    }

    public static class CausalityTracker {
        private final Map<Long, Timestamp> latestRemoteTimestamps = new ConcurrentHashMap<>();

        public void recordObservation(Timestamp remoteTimestamp) {
            latestRemoteTimestamps.merge(remoteTimestamp.getNodeId(), remoteTimestamp, (existing, incoming) -> {
                return incoming.compareTo(existing) > 0 ? incoming : existing;
            });
        }

        public boolean validateCausality(Timestamp localEvent, Timestamp observedRemote) {
            Timestamp latestKnown = latestRemoteTimestamps.get(observedRemote.getNodeId());
            if (latestKnown == null) {
                return true;
            }
            return observedRemote.compareTo(latestKnown) <= 0;
        }

        public void assertCausalOrder(Timestamp eventA, Timestamp eventB) {
            if (eventA.compareTo(eventB) > 0) {
                throw new IllegalStateException("Causal consistency violation: Event A " + eventA + " is strictly greater than Event B " + eventB);
            }
        }
    }

    public static class ChronosSerializer {
        private static final int MAGIC = 0x4348524E; // "CHRN"

        public static byte[] serialize(Timestamp timestamp, String payload) {
            byte[] payloadBytes = payload.getBytes(StandardCharsets.UTF_8);
            int capacity = 4 + 8 + 8 + 4 + 4 + payloadBytes.length;
            ByteBuffer buffer = ByteBuffer.allocate(capacity);
            buffer.putInt(MAGIC);
            buffer.putLong(timestamp.getNodeId());
            buffer.putLong(timestamp.getPhysical());
            buffer.putInt(timestamp.getLogical());
            buffer.putInt(payloadBytes.length);
            buffer.put(payloadBytes);
            return buffer.array();
        }

        public static SerializedMessage deserialize(byte[] data) {
            ByteBuffer buffer = ByteBuffer.wrap(data);
            int magic = buffer.getInt();
            if (magic != MAGIC) {
                throw new IllegalArgumentException("Invalid magic number in serialized Chronos message: " + Integer.toHexString(magic));
            }
            long nodeId = buffer.getLong();
            long physical = buffer.getLong();
            int logical = buffer.getInt();
            int payloadLen = buffer.getInt();
            byte[] payloadBytes = new byte[payloadLen];
            buffer.get(payloadBytes);
            String payload = new String(payloadBytes, StandardCharsets.UTF_8);
            Timestamp timestamp = new Timestamp(physical, logical, nodeId);
            return new SerializedMessage(timestamp, payload);
        }
    }

    public static class SerializedMessage {
        private final Timestamp timestamp;
        private final String payload;

        public SerializedMessage(Timestamp timestamp, String payload) {
            this.timestamp = timestamp;
            this.payload = payload;
        }

        public Timestamp getTimestamp() { return timestamp; }
        public String getPayload() { return payload; }
    }

    public static class NetworkSimulator {
        private final ExecutorService executor = Executors.newCachedThreadPool();
        private final BlockingQueue<byte[]> inbox = new LinkedBlockingQueue<>();
        private final Random random = new Random(42);

        public void send(byte[] packet, long artificialDelayMs) {
            executor.submit(() -> {
                try {
                    if (artificialDelayMs > 0) {
                        Thread.sleep(artificialDelayMs);
                    }
                    inbox.put(packet);
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                }
            });
        }

        public byte[] poll(long timeoutMs) throws InterruptedException {
            return inbox.poll(timeoutMs, TimeUnit.MILLISECONDS);
        }

        public void shutdown() {
            executor.shutdownNow();
        }
    }
}
