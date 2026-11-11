package factory;

public class Main {
    public static void main(String[] args) {
        System.out.println("=== ApexSim Engine CLI ===");
        Core.SimulationEngine engine = new Core.SimulationEngine();
        
        // Parse a sample DSL script
        String script = "# Sample Simulation Script\n"
                + "ACTOR name=NodeAlpha init=100 trigger=onTick\n"
                + "ACTOR name=NodeBeta init=200 trigger=onEvent\n";
        
        Core.DSLScript dsl = new Core.DSLScript();
        dsl.parse(script);
        System.out.println("Parsed actors from DSL: " + dsl.getActors().size());
        
        // Schedule some sample events
        engine.schedule(engine.createEvent(100, "NodeAlpha", "InitState"));
        engine.schedule(engine.createEvent(50, "NodeBeta", "FastEvent"));
        engine.schedule(engine.createEvent(200, "NodeAlpha", "FinalState"));
        
        System.out.println("Initial queue size: " + engine.getQueue().size());
        
        // Run simulation
        engine.runSimulation();
        
        System.out.println("Simulation completed.");
        System.out.println("Events processed: " + engine.getMetrics().getEventCount());
        System.out.println("Audit log record count: " + engine.getAuditLog().getRecordCount());
        System.out.println("Average queue depth: " + engine.getMetrics().getAverageQueueDepth());
    }
}
