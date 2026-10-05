package factory;

public class Main {
    public static void main(String[] args) {
        System.out.println("Starting ChronosEngine System Daemon...");
        long nodeId = 1001L;
        Core.ClockDriftDetector driftDetector = new Core.ClockDriftDetector(5000);
        Core.HybridLogicalClock hlc = new Core.HybridLogicalClock(nodeId, driftDetector);
        Core.CausalityTracker tracker = new Core.CausalityTracker();

        Core.Timestamp t1 = hlc.tick();
        System.out.println("Generated local event 1: " + t1);

        byte[] serialized = Core.ChronosSerializer.serialize(t1, "System Initialization Event");
        System.out.println("Serialized packet size: " + serialized.length + " bytes");

        Core.SerializedMessage deserialized = Core.ChronosSerializer.deserialize(serialized);
        System.out.println("Deserialized successfully: payload='" + deserialized.getPayload() + "', ts=" + deserialized.getTimestamp());

        Core.Timestamp remoteTs = new Core.Timestamp(t1.getPhysical() + 10, 0, 2002L);
        Core.Timestamp t2 = hlc.update(remoteTs);
        System.out.println("Updated HLC upon receiving remote message: " + t2);

        tracker.recordObservation(remoteTs);
        boolean causalValid = tracker.validateCausality(t2, remoteTs);
        System.out.println("Causality validation status: " + causalValid);

        System.out.println("ChronosEngine daemon check completed successfully.");
    }
}
