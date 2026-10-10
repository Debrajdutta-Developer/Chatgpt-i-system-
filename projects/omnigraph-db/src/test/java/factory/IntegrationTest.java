package factory;

import java.util.List;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.Callable;

public class IntegrationTest {

    private static int assertionsPassed = 0;

    private static void check(boolean condition, String message) {
        if (!condition) {
            throw new AssertionError("Integration assertion failed: " + message);
        }
        assertionsPassed++;
        System.out.println("PASS-INT: " + message);
    }

    public static void main(String[] args) throws Exception {
        System.out.println("Running IntegrationTest Suite...");
        
        Core.GraphDatabase db = new Core.GraphDatabase();
        
        // Test 1: Graph cycle detection (No cycle initially)
        Core.Node n1 = db.createNode("Server");
        Core.Node n2 = db.createNode("Server");
        Core.Node n3 = db.createNode("Server");
        db.createEdge(n1.getId(), n2.getId(), "CONNECTS");
        db.createEdge(n2.getId(), n3.getId(), "CONNECTS");
        
        check(!db.detectCycle(), "Linear graph has no cycles detected");
        
        // Test 2: Graph cycle detection (Introducing cycle)
        Core.Edge cycleEdge = db.createEdge(n3.getId(), n1.getId(), "CONNECTS");
        check(db.detectCycle(), "Cyclic graph correctly triggers cycle detection");
        
        // Test 3: Invalid edge deletion rollback / removal verification
        boolean removedCycleEdge = db.deleteEdge(cycleEdge.getId());
        check(removedCycleEdge, "Cycle edge successfully removed");
        check(!db.detectCycle(), "Cycle is resolved after edge removal");
        
        // Test 4: BFS Pathfinding verification
        List<Long> path = db.bfsPath(n1.getId(), n3.getId());
        check(path.size() == 3, "BFS finds path of length 3 between n1 and n3");
        check(path.get(0) == n1.getId(), "BFS path starts at n1");
        check(path.get(2) == n3.getId(), "BFS path ends at n3");
        
        // Test 5: Concurrent multi-hop pathfinding under race conditions using Virtual Threads
        try (var executor = Executors.newVirtualThreadPerTaskExecutor()) {
            Callable<List<Long>> task = () -> db.bfsPath(n1.getId(), n3.getId());
            Future<List<Long>> f1 = executor.submit(task);
            Future<List<Long>> f2 = executor.submit(task);
            List<Long> r1 = f1.get();
            List<Long> r2 = f2.get();
            check(r1.size() == 3, "Virtual thread concurrent pathfinding result 1 is valid");
            check(r2.size() == 3, "Virtual thread concurrent pathfinding result 2 is valid");
        }
        
        // Test 6: Malformed Cypher syntax throwing explicit parse errors
        boolean malformedCaught1 = false;
        try {
            Core.CypherParser.parse("");
        } catch (IllegalArgumentException e) {
            malformedCaught1 = true;
        }
        check(malformedCaught1, "Blank Cypher query throws parse error");
        
        // Test 7: Malformed CREATE syntax
        boolean malformedCaught2 = false;
        try {
            Core.CypherParser.parse("CREATE invalidSyntax");
        } catch (IllegalArgumentException e) {
            malformedCaught2 = true;
        }
        check(malformedCaught2, "Malformed CREATE syntax throws parse error");
        
        // Test 8: Property map store verification
        n1.getProperties().put("ip", new Core.PropertyValue("192.168.1.1"));
        check(n1.getProperties().containsKey("ip"), "Node stores property successfully");
        check(n1.getProperties().get("ip").getType() == Core.PropertyType.STRING, "Stored property type is STRING");
        check(n1.getProperties().get("ip").getValue().equals("192.168.1.1"), "Stored property value matches expected string");
        
        System.out.println("All IntegrationTest assertions passed successfully (" + assertionsPassed + " checks).");
    }
}
