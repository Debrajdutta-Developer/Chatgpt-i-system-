package factory;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.*;

public class Main {
    public static void main(String[] args) {
        System.out.println("Starting ChronoGraph DB Engine CLI...");
        Path tempLog;
        try {
            tempLog = Files.createTempFile("chronograph_main", ".log");
            tempLog.toFile().deleteOnExit();
        } catch (IOException e) {
            System.err.println("Failed to initialize temporary log file: " + e.getMessage());
            return;
        }

        try (Core.ChronoGraphEngine engine = new Core.ChronoGraphEngine(tempLog)) {
            System.out.println("Initialized ChronoGraph Engine with append-only log: " + tempLog);

            engine.appendVertex("NodeA", 100, 300, Collections.singletonMap("type", "Asset"));
            engine.appendVertex("NodeB", 100, 300, Collections.singletonMap("type", "Asset"));
            engine.appendEdge("EdgeAB", "NodeA", "NodeB", 100, 300, 1.5, Collections.singletonMap("rel", "OWNS"));

            System.out.println("Appended sample temporal vertices and edges.");

            Core.VertexRecord vr = engine.getVertexAt("NodeA", 200);
            System.out.println("Query NodeA at t=200: id=" + (vr != null ? vr.getId() : "null"));

            List<String> path = engine.findPathDijkstra("NodeA", "NodeB", 200);
            System.out.println("Computed Dijkstra path at t=200 size: " + path.size());

            // Virtual threads execution
            try (ExecutorService exec = Executors.newVirtualThreadPerTaskExecutor()) {
                Future<List<String>> f = exec.submit(() -> engine.findPathBFS("NodeA", "NodeB", 200));
                List<String> bfsPath = f.get(2, TimeUnit.SECONDS);
                System.out.println("Virtual thread BFS path size: " + bfsPath.size());
            } catch (Exception ex) {
                System.err.println("Virtual thread task failed: " + ex.getMessage());
            }

        } catch (IOException e) {
            System.err.println("Engine execution error: " + e.getMessage());
        }

        System.out.println("ChronoGraph DB Engine demonstration completed successfully.");
    }
}
