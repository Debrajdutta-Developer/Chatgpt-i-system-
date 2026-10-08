package factory;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.*;

public class IntegrationTest {

    private static int assertionsPassed = 0;

    private static void check(boolean condition, String message) {
        if (!condition) {
            throw new AssertionError("Assertion failed: " + message);
        }
        assertionsPassed++;
        System.out.println("  [PASS] " + message);
    }

    public static void main(String[] args) {
        System.out.println("Running IntegrationTest suite...");
        try {
            testConcurrentVirtualThreadReaders();
            testTemporalSnapshotIsolation();
            testComplexGraphTopologies();
            System.out.println("All IntegrationTest checks passed successfully! Total assertions: " + assertionsPassed);
        } catch (Exception e) {
            System.err.println("IntegrationTest failed with exception: " + e.getMessage());
            e.printStackTrace();
            System.exit(1);
        }
    }

    private static void testConcurrentVirtualThreadReaders() throws Exception {
        Path log = Files.createTempFile("integration_vt", ".log");
        log.toFile().deleteOnExit();

        try (Core.ChronoGraphEngine engine = new Core.ChronoGraphEngine(log)) {
            engine.appendVertex("NodeA", 0, 1000, Collections.emptyMap());
            engine.appendVertex("NodeB", 0, 1000, Collections.emptyMap());
            engine.appendEdge("EdgeAB", "NodeA", "NodeB", 0, 1000, 1.0, Collections.emptyMap());

            try (ExecutorService exec = Executors.newVirtualThreadPerTaskExecutor()) {
                Callable<Boolean> task = () -> {
                    for (int i = 0; i < 50; i++) {
                        long snapshotTime = 500;
                        Core.VertexRecord vr = engine.getVertexAt("NodeA", snapshotTime);
                        if (vr == null) return false;
                        List<String> path = engine.findPathBFS("NodeA", "NodeB", snapshotTime);
                        if (path.isEmpty()) return false;
                    }
                    return true;
                };

                Future<Boolean> f1 = exec.submit(task);
                Future<Boolean> f2 = exec.submit(task);
                Future<Boolean> f3 = exec.submit(task);

                check(f1.get(3, TimeUnit.SECONDS), "Virtual thread reader task 1 succeeded");
                check(f2.get(3, TimeUnit.SECONDS), "Virtual thread reader task 2 succeeded");
                check(f3.get(3, TimeUnit.SECONDS), "Virtual thread reader task 3 succeeded");
            }
        }
    }

    private static void testTemporalSnapshotIsolation() throws IOException {
        Path log = Files.createTempFile("integration_snap", ".log");
        log.toFile().deleteOnExit();

        try (Core.ChronoGraphEngine engine = new Core.ChronoGraphEngine(log)) {
            // Asset ownership transfer over time
            engine.appendVertex("Asset1", 0, 1000, Collections.emptyMap());
            engine.appendVertex("OwnerAlice", 0, 1000, Collections.emptyMap());
            engine.appendVertex(
                "OwnerBob", 0, 1000, Collections.emptyMap());

            // Alice owns Asset1 from t=100 to t=200
            engine.appendEdge("Own1", "OwnerAlice", "Asset1", 100, 200, 1.0, Collections.emptyMap());
            // Bob owns Asset1 from t=200 to t=300
            engine.appendEdge("Own2", "OwnerBob", "Asset1", 200, 300, 1.0, Collections.emptyMap());

            List<String> alicePath = engine.findPathBFS("OwnerAlice", "Asset1", 150);
            List<String> bobPathAt150 = engine.findPathBFS("OwnerBob", "Asset1", 150);
            List<String> bobPathAt250 = engine.findPathBFS("OwnerBob", "Asset1", 250);
            List<String> alicePathAt250 = engine.findPathBFS("OwnerAlice", "Asset1", 250);

            check(alicePath.size() == 2, "Alice connected to Asset1 at t=150");
            check(bobPathAt150.isEmpty(), "Bob not connected to Asset1 at t=150");
            check(bobPathAt250.size() == 2, "Bob connected to Asset1 at t=250");
            check(alicePathAt250.isEmpty(), "Alice not connected to Asset1 at t=250");
        }
    }

    private static void testComplexGraphTopologies() throws IOException {
        Path log = Files.createTempFile(
            "integration_topo", ".log");
        log.toFile().deleteOnExit();

        try (Core.ChronoGraphEngine engine = new Core.ChronoGraphEngine(log)) {
            engine.appendVertex("X", 0, 1000, Collections.emptyMap());
            engine.appendVertex("Y", 0, 1000, Collections.emptyMap());
            engine.appendVertex("Z", 0, 1000, Collections.emptyMap());
            engine.appendEdge("EXY", "X", "Y", 0, 1000, 10.0, Collections.emptyMap());
            engine.appendEdge("EYZ", "Y", "Z", 0, 1000, 10.0, Collections.emptyMap());

            List<String> bfsResult = engine.findPathBFS("X", "Z", 500);
            List<String> dijkstraResult = engine.findPathDijkstra("X", "Z", 500);

            check(bfsResult.size() == 3, "BFS finds 3-node path X->Y->Z");
            check(dijkstraResult.size() == 3, "Dijkstra finds 3-node path X->Y->Z");
            check("Z".equals(dijkstraResult.get(2)), "Dijkstra path terminates at Z");
        }
    }
}
