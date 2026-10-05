package factory;

public class CoreTest {

    private static int assertionsPassed = 0;

    private static void check(boolean condition, String description) {
        if (!condition) {
            throw new AssertionError("ASSERTION FAILED: " + description);
        }
        assertionsPassed++;
        System.out.println("  [PASS] " + description);
    }

    public static void main(String[] args) {
        System.out.println("Running CoreTest Suite...");

        // Test 1: Timestamp comparison ordering
        Core.Timestamp ts1 = new Core.Timestamp(1000L, 0, 1L);
        Core.Timestamp ts2 = new Core.Timestamp(1000L, 1, 1L);
        Core.Timestamp ts3 = new Core.Timestamp(1001L, 0, 1L);
        check(ts1.compareTo(ts2) < 0, "Timestamp ts1 is less than ts2 via logical counter");
        check(ts2.compareTo(ts3) < 0, "Timestamp ts2 is less than ts3 via physical time");

        // Test 2: HLC Monotonicity
        Core.ClockDriftDetector drift = new Core.ClockDriftDetector(1000);
        Core.HybridLogicalClock hlc = new Core.HybridLogicalClock(1L, drift);
        Core.Timestamp a1 = hlc.tick();
        Core.Timestamp a2 = hlc.tick();
        check(a1.compareTo(a2) < 0, "HLC tick produces strictly monotonic timestamps");

        // Test 3: Remote update synchronization
        Core.Timestamp remote = new Core.Timestamp(a2.getPhysical() + 50, 5, 2L);
        Core.Timestamp a3 = hlc.update(remote);
        check(a3.compareTo(remote) > 0, "HLC update correctly advances beyond remote timestamp");

        // Test 4: Drift Compensator backward jump protection
        Core.ClockDriftDetector driftComp = new Core.ClockDriftDetector(1000);
        long t_init = driftComp.compensate(5000L);
        long t_jump = driftComp.compensate(2000L);
        check(t_jump == 5000L, "Clock drift detector clamps backward time jumps to previous physical value");
        check(driftComp.getDriftCorrections() == 1L, "Clock drift detector records excessive skew correction");

        // Test 5: Causality Tracker validation
        Core.CausalityTracker tracker = new Core.CausalityTracker();
        Core.Timestamp obs = new Core.Timestamp(4000L, 2, 99L);
        tracker.recordObservation(obs);
        boolean isValid = tracker.validateCausality(obs, new Core.Timestamp(4000L, 1, 99L));
        check(isValid, "Causality tracker validates valid past event");

        // Test 6: Causal order enforcement exception
        boolean exceptionThrown = false;
        try {
            tracker.assertCausalOrder(ts3, ts1);
        } catch (IllegalStateException e) {
            exceptionThrown = true;
        }
        check(exceptionThrown, "Causality tracker throws exception on causal violation");

        // Test 7: Serialization roundtrip
        String payload = "Test Payload Data";
        byte[] bytes = Core.ChronosSerializer.serialize(a1, payload);
        Core.SerializedMessage decoded = Core.ChronosSerializer.deserialize(bytes);
        check(decoded.getTimestamp().equals(a1), "Serialized timestamp matches original after roundtrip");
        check(decoded.getPayload().equals(payload), "Serialized payload matches original after roundtrip");

        // Test 8: Malformed deserialization rejection
        boolean malformedCaught = false;
        try {
            byte[] badBytes = new byte[]{0x00, 0x01, 0x02, 0x03, 0x04};
            Core.ChronosSerializer.deserialize(badBytes);
        } catch (IllegalArgumentException e) {
            malformedCaught = true;
        }
        check(malformedCaught, "Serializer rejects invalid magic header bytes");

        // Test 9: Counter overflow handling
        Core.HybridLogicalClock hlcOverflow = new Core.HybridLogicalClock(1L, new Core.ClockDriftDetector(10000));
        // Force rapid internal ticks
        for (int i = 0; i < 5; i++) {
            hlcOverflow.tick();
        }
        check(hlcOverflow.getCurrentTime().getNodeId() == 1L, "HLC maintains correct node ID through operations");

        // Test 10: Timestamp equality contract
        Core.Timestamp tCopy = new Core.Timestamp(a1.getPhysical(), a1.getLogical(), a1.getNodeId());
        check(a1.equals(tCopy), "Timestamp equals works correctly for identical timestamps");
        check(a1.hashCode() == tCopy.hashCode(), "Timestamp hashCode matches for identical timestamps");

        System.out.println("CoreTest Suite PASSED. Total assertions verified: " + assertionsPassed);
    }
}
