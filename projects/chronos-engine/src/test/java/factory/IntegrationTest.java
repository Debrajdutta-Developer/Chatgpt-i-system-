package factory;

public class IntegrationTest {

    private static int assertionsPassed = 0;

    private static void check(boolean condition, String description) {
        if (!condition) {
            throw new AssertionError("INTEGRATION ASSERTION FAILED: " + description);
        }
        assertionsPassed++;
        System.out.println("  [PASS] " + description);
    }

    public static void main(String[] args) throws Exception {
        System.out.println("Running IntegrationTest Suite...");

        // Integration 1: Multi-node network simulation with causal synchronization
        Core.ClockDriftDetector drift1 = new Core.ClockDriftDetector(2000);
        Core.ClockDriftDetector drift2 = new Core.ClockDriftDetector(2000);

        Core.HybridLogicalClock nodeA = new Core.HybridLogicalClock(101L, drift1);
        Core.HybridLogicalClock nodeB = new Core.HybridLogicalClock(102L, drift2);

        Core.NetworkSimulator netSim = new Core.NetworkSimulator();

        Core.Timestamp tA1 = nodeA.tick();
        byte[] packetA = Core.ChronosSerializer.serialize(tA1, "Hello from A");
        netSim.send(packetA, 10);

        byte[] receivedPacket = netSim.poll(1000);
        check(receivedPacket != null, "Network simulator successfully delivers packet");

        Core.SerializedMessage decodedA = Core.ChronosSerializer.deserialize(receivedPacket);
        check(decodedA.getTimestamp().equals(tA1), "Delivered packet timestamp matches node A tick");

        Core.Timestamp tB1 = nodeB.update(decodedA.getTimestamp());
        check(tB1.compareTo(tA1) > 0, "Node B HLC advances strictly ahead of received timestamp from Node A");

        // Integration 2: Bidirectional message exchange causality check
        Core.Timestamp tB2 = nodeB.tick();
        byte[] packetB = Core.ChronosSerializer.serialize(tB2, "Response from B");
        netSim.send(packetB, 5);

        byte[] receivedB = netSim.poll(1000);
        Core.SerializedMessage decodedB = Core.ChronosSerializer.deserialize(receivedB);
        Core.Timestamp tA2 = nodeA.update(decodedB.getTimestamp());
        check(tA2.compareTo(tB2) > 0, "Node A HLC advances after receiving response from Node B");

        // Integration 3: Causality Tracker distributed invariant enforcement
        Core.CausalityTracker globalTracker = new Core.CausalityTracker();
        globalTracker.recordObservation(tA1);
        globalTracker.recordObservation(tB1);
        globalTracker.recordObservation(tA2);
        globalTracker.recordObservation(tB2);

        boolean orderValid = globalTracker.validateCausality(tA2, tB2);
        check(orderValid, "Causality tracker validates valid concurrent/subsequent observations");

        // Integration 4: Concurrent tick stress test
        int threadCount = 4;
        int ticksPerThread = 50;
        java.util.concurrent.ExecutorService pool = java.util.concurrent.Executors.newFixedThreadPool(threadCount);
        java.util.List<java.util.concurrent.Future<java.util.List<Core.Timestamp>>> futures = new java.util.ArrayList<>();

        for (int i = 0; i < threadCount; i++) {
            final long id = 200L + i;
            futures.add(pool.submit(() -> {
                Core.HybridLogicalClock localHlc = new Core.HybridLogicalClock(id, new Core.ClockDriftDetector(1000));
                java.util.List<Core.Timestamp> localList = new java.util.ArrayList<>();
                for (int j = 0; j < ticksPerThread; j++) {
                    localList.add(localHlc.tick());
                }
                return localList;
            }));
        }

        pool.shutdown();
        boolean poolFinished = pool.awaitTermination(5, java.util.concurrent.TimeUnit.SECONDS);
        check(poolFinished, "Concurrent HLC thread pool completes execution successfully");

        int totalTimestampsCollected = 0;
        for (var f : futures) {
            java.util.List<Core.Timestamp> list = f.get();
            check(list.size() == ticksPerThread, "Each concurrent thread produces exact expected number of timestamps");
            totalTimestampsCollected += list.size();
            // Verify intra-thread monotonicity
            for (int k = 1; k < list.size(); k++) {
                check(list.get(k - 1).compareTo(list.get(k)) < 0, "Intra-thread timestamps are strictly monotonic");
            }
        }
        check(totalTimestampsCollected == threadCount * ticksPerThread, "Total collected concurrent timestamps matches expectation");

        netSim.shutdown();
        System.out.println("IntegrationTest Suite PASSED. Total assertions verified: " + assertionsPassed);
    }
}
