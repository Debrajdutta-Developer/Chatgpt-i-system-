package factory;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Collections;
import java.util.List;

public class CoreTest {

    private static int assertionsPassed = 0;

    private static void check(boolean condition, String message) {
        if (!condition) {
            throw new AssertionError("Assertion failed: " + message);
        }
        assertionsPassed++;
        System.out.println("  [PASS] " + message);
    }

    public static void main(String[] args) {
        System.out.println("Running CoreTest suite...");
        try {
            testTemporalVertexVersioning();
            testTemporalEdgeVersioning();
            testCrashRecoveryAndLogIntegrity();
            testDijkstraPathfinding();
            System.out.println("All CoreTest checks passed successfully! Total assertions: " + assertionsPassed);
        } catch (Exception e) {
            System.err.println("CoreTest failed with exception: " + e.getMessage());
            e.printStackTrace();
            System.exit(1);
        }
    }

    private static void testTemporalVertexVersioning() throws IOException {
        Path log = Files.createTempFile("core_test_v", ".log");
        log.toFile().deleteOnExit();

        try (Core.ChronoGraphEngine engine = new Core.ChronoGraphEngine(log)) {
            engine.appendVertex("V1", 100, 200, Collections.singletonMap("val", "old"));
            engine.appendVertex("V1", 200, 300, Collections.singletonMap("val", "new"));

            Core.VertexRecord v1At150 = engine.getVertexAt("V1", 150);
            Core.VertexRecord v1At250 = engine.getVertexAt("V1", 250);
            Core.VertexRecord v1Before = engine.getVertexAt("V1", 50);
            Core.VertexRecord v1After = engine.getVertexAt("V1", 350);

            check(v1At150 != null, "Vertex V1 exists at t=150");
            check("old".equals(v1At150.getProperties().get("val")), "Vertex V1 has 'old' property at t=150");
            check(v1At250 != null, "Vertex V1 exists at t=250");
            check("new".equals(v1At250.getProperties().get("val")), "Vertex V1 has 'new' property at t=250");
            check(v1Before == null, "Vertex V1 does not exist before t=100");
            check(v1After == null, "Vertex V1 does not exist after t=300");
        }
    }

    private static void testTemporalEdgeVersioning() throws IOException {
        Path log = Files.createTempFile("core_test_e", ".log");
        log.toFile().deleteOnExit();

        try (Core.ChronoGraphEngine engine = new Core.ChronoGraphEngine(log)) {
            engine.appendVertex("A", 0, 1000, Collections.emptyMap());
            engine.appendVertex("B", 0, 1000, Collections.emptyMap());
            engine.appendEdge("E1", "A", "B", 100, 200, 5.0, Collections.emptyMap());
            engine.appendEdge("E1", "A", "B", 200, 300, 2.0, Collections.emptyMap());

            Core.EdgeRecord e1At150 = engine.getEdgeAt("E1", 150);
            Core.EdgeRecord e1At250 = engine.getEdgeAt("E1", 250);
            Core.EdgeRecord e1At50 = engine.getEdgeAt("E1", 50);

            check(e1At150 != null, "Edge E1 exists at t=150");
            check(e1At150.getWeight() == 5.0, "Edge E1 weight is 5.0 at t=150");
            check(e1At250 != null, "Edge E1 exists at t=250");
            check(e1At250.getWeight() == 2.0, "Edge E1 weight is 2.0 at t=250");
            check(e1At50 == null, "Edge E1 does not exist at t=50");
        }
    }

    private static void testCrashRecoveryAndLogIntegrity() throws IOException {
        Path log = Files.createTempFile("core_test_rec", ".log");
        log.toFile().deleteOnExit();

        try (Core.ChronoGraphEngine engine = new Core.ChronoGraphEngine(log)) {
            engine.appendVertex("V2", 100, 300, Collections.singletonMap("status", "active"));
            engine.appendEdge("E1", "V2", "V2", 100, 300, 1.0, Collections.emptyMap());
        }

        // Recover in new engine instance
        try (Core.ChronoGraphEngine recoveredEngine = new Core.ChronoGraphEngine(log)) {
            Core.VertexRecord recoveredV2 = recoveredEngine.getVertexAt("V2", 200);
            Core.EdgeRecord recoveredE1 = recoveredEngine.getEdgeAt("E1", 200);

            check(recoveredV2 != null, "Recovered vertex V2 exists at t=200");
            check("active".equals(recoveredV2.getProperties().get("status")), "Recovered vertex property intact");
            check(recoveredE1 != null, "Recovered edge E1 exists at t=200");
            check(recoveredE1.getWeight() == 1.0, "Recovered edge weight intact");
        }
    }

    private static void testDijkstraPathfinding() throws IOException {
        Path log = Files.createTempFile(
            "core_test_dijkstra", ".log");
        log.toFile().deleteOnExit();

        try (Core.ChronoGraphEngine engine = new Core.ChronoGraphEngine(log)) {
            engine.appendVertex("N1", 0, 1000, Collections.emptyMap());
            engine.appendVertex("N2", 0, 1000, Collections.emptyMap());
            engine.appendVertex("N3", 0, 1000, Collections.emptyMap());

            engine.appendEdge("Path1", "N1", "N2", 0, 1000, 1.0, Collections.emptyMap());
            engine.appendEdge("Path2", "N2", "N3", 0, 1000, 2.0, Collections.emptyMap());
            engine.appendEdge("Path3", "N1", "N3", 0, 1000, 5.0, Collections.emptyMap());

            List<String> shortestPath = engine.findPathDijkstra("N1", "N3", 500);

            check(shortestPath.size() == 3, "Dijkstra shortest path contains 3 nodes");
            check("N1".equals(shortestPath.get(0)), "Path starts at N1");
            check("N2".equals(shortestPath.get(1)), "Path goes through N2");
            check("N3".equals(shortestPath.get(2)), "Path ends at N3");
        }
    }
}
