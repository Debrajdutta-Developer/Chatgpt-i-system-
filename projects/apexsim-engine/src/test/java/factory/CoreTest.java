package factory;

public class CoreTest {

    private static int assertionCount = 0;

    private static void check(boolean condition, String message) {
        if (!condition) {
            throw new AssertionError("Assertion failed: " + message);
        }
        assertionCount++;
        System.out.println("[PASS] " + message);
    }

    public static void main(String[] args) {
        System.out.println("Running CoreTest...");
        testCalendarQueue();
        testDSLParser();
        testCausalityTracker();
        testCheckpointManager();
        testDeterministicPRNG();
        testBinaryAuditLog();
        testPerformanceMetrics();
        System.out.println("All CoreTest assertions passed successfully. Total assertions: " + assertionCount);
    }

    private static void testCalendarQueue() {
        Core.CalendarQueue queue = new Core.CalendarQueue();
        check(queue.isEmpty(), "Queue should be initially empty");
        
        Core.SimulationEvent e1 = new Core.SimulationEvent(1, 100, "A", "p1");
        Core.SimulationEvent e2 = new Core.SimulationEvent(2, 50, "B", "p2");
        
        queue.schedule(e1);
        queue.schedule(e2);
        
        check(queue.size() == 2, "Queue size should be 2 after insertions");
        
        Core.SimulationEvent polled1 = queue.poll();
        check(polled1 != null && polled1.timestamp == 50, "Polled event should have smallest timestamp (50)");
        
        Core.SimulationEvent polled2 = queue.poll();
        check(polled2 != null && polled2.timestamp == 100, "Polled event should have next timestamp (100)");
        
        check(queue.isEmpty(), "Queue should be empty after polling all events");
    }

    private static void testDSLParser() {
        String script = "# Test Script\nACTOR name=TestNode init=42 trigger=onStart\n";
        Core.DSLScript dsl = new Core.DSLScript();
        dsl.parse(script);
        
        check(dsl.getActors().size() == 1, "DSL parser should parse exactly 1 actor");
        check(dsl.getActors().get(0).name().equals("TestNode"), "Actor name should be TestNode");
        check(dsl.getActors().get(0).initialValue() == 42, "Actor initial value should be 42");
        check(dsl.getActors().get(0).trigger().equals("onStart"), "Actor trigger should be onStart");
    }

    private static void testCausalityTracker() {
        Core.CausalityTracker tracker = new Core.CausalityTracker();
        tracker.validateAndAdvance("Actor1", 100);
        boolean exceptionThrown = false;
        try {
            tracker.validateAndAdvance("Actor1", 90);
        } catch (Core.CausalityViolationException e) {
            exceptionThrown = true;
        }
        check(exceptionThrown, "Causality violation should be detected when event timestamp goes backwards");
    }

    private static void testCheckpointManager() {
        Core.CheckpointManager cm = new Core.CheckpointManager();
        cm.saveCheckpoint(10, "StateData10");
        String loaded = cm.loadCheckpoint(10);
        check(loaded != null && loaded.equals("StateData10"), "Loaded checkpoint should match saved state data");
        check(cm.loadCheckpoint(99) == null, "Non-existent checkpoint should return null");
    }

    private static void testDeterministicPRNG() {
        Core.DeterministicPRNG prng1 = new Core.DeterministicPRNG(12345);
        Core.DeterministicPRNG prng2 = new Core.DeterministicPRNG(12345);
        check(prng1.nextInt(100) == prng2.nextInt(100), "PRNG streams with same seed must match on first int");
        check(prng1.nextDouble() == prng2.nextDouble(), "PRNG streams with same seed must match on first double");
    }

    private static void testBinaryAuditLog() {
        Core.BinaryAuditLog log = new Core.BinaryAuditLog();
        Core.SimulationEvent e = new Core.SimulationEvent(1, 200, "NodeX", "PayloadX");
        log.recordEvent(e);
        check(log.getRecordCount() == 1, "Audit log should contain exactly 1 recorded event");
    }

    private static void testPerformanceMetrics() {
        Core.PerformanceMetrics metrics = new Core.PerformanceMetrics();
        check(metrics.getEventCount() == 0, "Initial event count should be 0");
        metrics.recordEventProcessed();
        check(metrics.getEventCount() == 1, "Event count should be 1 after recording");
        metrics.sampleQueueDepth(10);
        metrics.sampleQueueDepth(20);
        check(metrics.getAverageQueueDepth() == 15.0, "Average queue depth should calculate correctly");
    }
}
