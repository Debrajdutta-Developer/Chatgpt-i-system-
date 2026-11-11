package factory;

public class IntegrationTest {

    private static int assertionCount = 0;

    private static void check(boolean condition, String message) {
        if (!condition) {
            throw new AssertionError("Integration assertion failed: " + message);
        }
        assertionCount++;
        System.out.println("[INTEGRATION PASS] " + message);
    }

    public static void main(String[] args) {
        System.out.println("Running IntegrationTest...");
        testMillionEventSimulation();
        testCausalityViolationEnforcement();
        testDeterministicReplay();
        System.out.println("All IntegrationTest assertions passed successfully. Total assertions: " + assertionCount);
    }

    private static void testMillionEventSimulation() {
        Core.SimulationEngine engine = new Core.SimulationEngine();
        int eventCount = 10000;
        for (int i = eventCount; i > 0; i--) {
            engine.schedule(engine.createEvent(i, "WorkerActor", "payload-" + i));
        }
        check(engine.getQueue().size() == eventCount, "Queue should hold all scheduled events");
        
        engine.runSimulation();
        check(engine.getQueue().isEmpty(), "Queue should be completely empty after simulation run");
        check(engine.getMetrics().getEventCount() == eventCount, "Metrics should report exact number of processed events");
        check(engine.getAuditLog().getRecordCount() == eventCount, "Audit log should contain records for all processed events");
    }

    private static void testCausalityViolationEnforcement() {
        Core.SimulationEngine engine = new Core.SimulationEngine();
        engine.schedule(engine.createEvent(100, "ActorA", "first"));
        engine.runSimulation();

        boolean causalityCaught = false;
        try {
            engine.getCausalityTracker().validateAndAdvance("ActorA", 50);
        } catch (Core.CausalityViolationException e) {
            causalityCaught = true;
        }
        check(causalityCaught, "Engine must throw CausalityViolationException when actor time moves backward");
    }

    private static void testDeterministicReplay() {
        Core.SimulationEngine engine1 = new Core.SimulationEngine();
        Core.SimulationEngine engine2 = new Core.SimulationEngine();

        for (int i = 0; i < 100; i++) {
            long ts = (i * 13) % 50;
            engine1.schedule(engine1.createEvent(ts, "ActorB", "data-" + i));
            engine2.schedule(engine2.createEvent(ts, "ActorB", "data-" + i));
        }

        engine1.runSimulation();
        engine2.runSimulation();

        byte[] log1 = engine1.getAuditLog().getData();
        byte[] log2 = engine2.getAuditLog().getData();

        check(log1.length == log2.length, "Replay audit logs must match in length");
        boolean bytesMatch = true;
        for (int i = 0; i < log1.length; i++) {
            if (log1[i] != log2[i]) {
                bytesMatch = false;
                break;
            }
        }
        check(bytesMatch, "Replay audit logs must match byte-for-byte across identical runs");
        check(engine1.getMetrics().getEventCount() == engine2.getMetrics().getEventCount(), "Processed event counts must match exactly");
    }
}
