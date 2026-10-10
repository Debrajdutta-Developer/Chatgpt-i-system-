package factory;

import java.util.List;

public class CoreTest {

    private static int assertionsPassed = 0;

    private static void check(boolean condition, String message) {
        if (!condition) {
            throw new AssertionError("Assertion failed: " + message);
        }
        assertionsPassed++;
        System.out.println("PASS: " + message);
    }

    public static void main(String[] args) {
        System.out.println("Running CoreTest Suite...");
        
        Core.GraphDatabase db = new Core.GraphDatabase();
        
        // Test 1: Node creation
        Core.Node n1 = db.createNode("Person");
        check(n1 != null, "Node n1 is successfully created");
        check(n1.getId() == 1L, "Node n1 ID is correctly assigned as 1");
        check(n1.getLabel().equals("Person"), "Node n1 label is Person");
        
        // Test 2: Second node creation
        Core.Node n2 = db.createNode("Person");
        check(n2.getId() == 2L, "Node n2 ID is correctly assigned as 2");
        
        // Test 3: Edge creation
        Core.Edge edge = db.createEdge(n1.getId(), n2.getId(), "KNOWS");
        check(edge != null, "Edge is successfully created between n1 and n2");
        check(edge.getSourceId() == n1.getId(), "Edge source ID matches n1");
        check(edge.getTargetId() == n2.getId(), "Edge target ID matches n2");
        check(edge.getType().equals("KNOWS"), "Edge type is KNOWS");
        
        // Test 4: Cypher parsing valid query
        String validQuery = "CREATE (p:Person {name:Bob})";
        Core.CypherParser.AST ast = Core.CypherParser.parse(validQuery);
        check(ast.getCommand().equals("CREATE"), "Cypher parser identifies CREATE command");
        check(ast.getLabel().equals("Person"), "Cypher parser identifies Person label");
        check(ast.getVariable().equals("p"), "Cypher parser identifies variable p");
        
        // Test 5: Cypher parsing malformed query
        boolean parseErrorThrown = false;
        try {
            Core.CypherParser.parse("INVALID (badquery)");
        } catch (IllegalArgumentException e) {
            parseErrorThrown = true;
        }
        check(parseErrorThrown, "Malformed Cypher query throws explicit parse error");
        
        // Test 6: Query planner generation
        List<String> plan = Core.QueryPlanner.plan(ast);
        check(plan.size() == 4, "Query planner generates 4 execution steps for CREATE");
        check(plan.get(0).equals("SCAN_CATALOG"), "First plan step is SCAN_CATALOG");
        
        // Test 7: Storage engine buffer capacity
        Core.StorageEngine storage = new Core.StorageEngine(1024);
        check(storage.capacity() == 1024, "Storage engine allocates correct direct buffer capacity");
        
        // Test 8: Property value type checking
        Core.PropertyValue pv = new Core.PropertyValue(42L);
        check(pv.getType() == Core.PropertyType.LONG, "PropertyValue correctly tags LONG type");
        check((long)pv.getValue() == 42L, "PropertyValue returns correct LONG value");
        
        // Test 9: Edge deletion
        boolean deleted = db.deleteEdge(edge.getId());
        check(deleted, "Edge is successfully deleted");
        check(db.getEdge(edge.getId()) == null, "Deleted edge is no longer in database");
        
        // Test 10: Journal recording
        List<String> journal = db.getJournal();
        check(journal.size() >= 3, "Journal records mutation events");
        check(journal.get(0).startsWith("CREATE_NODE"), "Journal records node creation event");
        
        System.out.println("All CoreTest assertions passed successfully (" + assertionsPassed + " checks).");
    }
}
