package factory;

import java.io.*;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.*;
import java.util.zip.CRC32;

public class Core {

    public static class SimulationException extends RuntimeException {
        private static final long serialVersionUID = 1L;
        public SimulationException(String message) { super(message); }
    }

    public static class CausalityViolationException extends SimulationException {
        private static final long serialVersionUID = 1L;
        public CausalityViolationException(String message) { super(message); }
    }

    public static class SimulationEvent implements Comparable<SimulationEvent> {
        private static final long serialVersionUID = 1L;
        public final long eventId;
        public final long timestamp;
        public final String actorId;
        public final String payload;

        public SimulationEvent(long eventId, long timestamp, String actorId, String payload) {
            this.eventId = eventId;
            this.timestamp = timestamp;
            this.actorId = actorId;
            this.payload = payload;
        }

        @Override
        public int compareTo(SimulationEvent o) {
            int cmp = Long.compare(this.timestamp, o.timestamp);
            if (cmp != 0) return cmp;
            return Long.compare(this.eventId, o.eventId);
        }
    }

    public static class CalendarQueue {
        private final ConcurrentSkipListMap<Long, Queue<SimulationEvent>> buckets = new ConcurrentSkipListMap<>();
        private final AtomicLong size = new AtomicLong(0);

        public void schedule(SimulationEvent event) {
            buckets.computeIfAbsent(event.timestamp, k -> new ConcurrentLinkedQueue<>()).add(event);
            size.incrementAndGet();
        }

        public SimulationEvent poll() {
            while (!buckets.isEmpty()) {
                Map.Entry<Long, Queue<SimulationEvent>> entry = buckets.firstEntry();
                if (entry == null) return null;
                Queue<SimulationEvent> q = entry.getValue();
                SimulationEvent event = q.poll();
                if (event != null) {
                    size.decrementAndGet();
                    if (q.isEmpty()) {
                        buckets.remove(entry.getKey());
                    }
                    return event;
                } else {
                    buckets.remove(entry.getKey());
                }
            }
            return null;
        }

        public long size() {
            return size.get();
        }

        public boolean isEmpty() {
            return size.get() == 0;
        }
    }

    public static class DSLScript {
        public record ActorDefinition(String name, int initialValue, String trigger) {}
        private final List<ActorDefinition> actors = new ArrayList<>();

        public void parse(String scriptContent) {
            String[] lines = scriptContent.split("\n");
            for (String line : lines) {
                line = line.trim();
                if (line.isEmpty() || line.startsWith("#")) continue;
                if (line.startsWith("ACTOR")) {
                    // Format: ACTOR name=NodeA init=10 trigger=onTick
                    String[] parts = line.split("\\s+");
                    String name = "";
                    int init = 0;
                    String trigger = "";
                    for (int i = 1; i < parts.length; i++) {
                        String[] kv = parts[i].split("=");
                        if (kv.length == 2) {
                            if (kv[0].equals("name")) name = kv[1];
                            else if (kv[0].equals("init")) init = Integer.parseInt(kv[1]);
                            else if (kv[0].equals("trigger")) trigger = kv[1];
                        }
                    }
                    if (!name.isEmpty()) {
                        actors.add(new ActorDefinition(name, init, trigger));
                    }
                }
            }
        }

        public List<ActorDefinition> getActors() {
            return actors;
        }
    }

    public static class CausalityTracker {
        private final ConcurrentHashMap<String, Long> actorLogicalTimes = new ConcurrentHashMap<>();

        public synchronized void validateAndAdvance(String actorId, long eventTimestamp) {
            long lastTime = actorLogicalTimes.getOrDefault(actorId, -1L);
            if (eventTimestamp < lastTime) {
                throw new CausalityViolationException("Causality violation for actor " + actorId + ": event time " + eventTimestamp + " is before last processed time " + lastTime);
            }
            actorLogicalTimes.put(actorId, eventTimestamp);
        }
    }

    public static class CheckpointManager {
        private final ConcurrentHashMap<Long, byte[]> snapshots = new ConcurrentHashMap<>();

        public void saveCheckpoint(long step, String stateData) {
            snapshots.put(step, stateData.getBytes(StandardCharsets.UTF_8));
        }

        public String loadCheckpoint(long step) {
            byte[] data = snapshots.get(step);
            if (data == null) return null;
            return new String(data, StandardCharsets.UTF_8);
        }
    }

    public static class DeterministicPRNG {
        private final Random random;

        public DeterministicPRNG(long seed) {
            this.random = new Random(seed);
        }

        public synchronized int nextInt(int bound) {
            return random.nextInt(bound);
        }

        public synchronized double nextDouble() {
            return random.nextDouble();
        }
    }

    public static class BinaryAuditLog {
        private final ByteArrayOutputStream buffer = new ByteArrayOutputStream();

        public synchronized void recordEvent(SimulationEvent event) {
            try {
                byte[] actorBytes = event.actorId.getBytes(StandardCharsets.UTF_8);
                byte[] payloadBytes = event.payload.getBytes(StandardCharsets.UTF_8);
                ByteBuffer header = ByteBuffer.allocate(24);
                header.putLong(event.eventId);
                header.putLong(event.timestamp);
                header.putInt(actorBytes.length);
                header.putInt(payloadBytes.length);

                CRC32 crc = new CRC32();
                crc.update(header.array());
                crc.update(actorBytes);
                crc.update(payloadBytes);
                long checksum = crc.getValue();

                buffer.write(header.array());
                buffer.write(actorBytes);
                buffer.write(payloadBytes);
                ByteBuffer crcBuf = ByteBuffer.allocate(8);
                crcBuf.putLong(checksum);
                buffer.write(crcBuf.array());
            } catch (IOException e) {
                throw new SimulationException("Failed to record event in audit log: " + e.getMessage());
            }
        }

        public byte[] getData() {
            return buffer.toByteArray();
        }

        public int getRecordCount() {
            // Read through buffer and count valid records
            byte[] data = buffer.toByteArray();
            int count = 0;
            int offset = 0;
            while (offset < data.length) {
                if (offset + 24 > data.length) break;
                ByteBuffer header = ByteBuffer.wrap(data, offset, 24);
                long eventId = header.getLong();
                long timestamp = header.getLong();
                int actorLen = header.getInt();
                int payloadLen = header.getInt();
                int totalRecordSize = 24 + actorLen + payloadLen + 8;
                if (offset + totalRecordSize > data.length) break;
                offset += totalRecordSize;
                count++;
            }
            return count;
        }
    }

    public static class PerformanceMetrics {
        private final AtomicLong eventCount = new AtomicLong(0);
        private final AtomicLong totalQueueDepth = new AtomicLong(0);
        private final AtomicLong queueSamples = new AtomicLong(0);

        public void recordEventProcessed() {
            eventCount.incrementAndGet();
        }

        public void sampleQueueDepth(long depth) {
            totalQueueDepth.addAndGet(depth);
            queueSamples.incrementAndGet();
        }

        public long getEventCount() {
            return eventCount.get();
        }

        public double getAverageQueueDepth() {
            long samples = queueSamples.get();
            if (samples == 0) return 0.0;
            return (double) totalQueueDepth.get() / samples;
        }
    }

    public static class SimulationEngine {
        private final CalendarQueue queue = new CalendarQueue();
        private final CausalityTracker causalityTracker = new CausalityTracker();
        private final CheckpointManager checkpointManager = new CheckpointManager();
        private final BinaryAuditLog auditLog = new BinaryAuditLog();
        private final PerformanceMetrics metrics = new PerformanceMetrics();
        private final AtomicLong eventIdCounter = new AtomicLong(0);

        public SimulationEvent createEvent(long timestamp, String actorId, String payload) {
            return new SimulationEvent(eventIdCounter.incrementAndGet(), timestamp, actorId, payload);
        }

        public void schedule(SimulationEvent event) {
            queue.schedule(event);
            metrics.sampleQueueDepth(queue.size());
        }

        public boolean step() {
            SimulationEvent event = queue.poll();
            if (event == null) return false;

            try {
                causalityTracker.validateAndAdvance(event.actorId, event.timestamp);
            } catch (CausalityViolationException e) {
                throw e;
            }

            auditLog.recordEvent(event);
            metrics.recordEventProcessed();
            metrics.sampleQueueDepth(queue.size());
            return true;
        }

        public void runSimulation() {
            while (step()) {
                // Process until empty
            }
        }

        public CalendarQueue getQueue() { return queue; }
        public CausalityTracker getCausalityTracker() { return causalityTracker; }
        public CheckpointManager getCheckpointManager() { return checkpointManager; }
        public BinaryAuditLog getAuditLog() { return auditLog; }
        public PerformanceMetrics getMetrics() { return metrics; }
    }
}
