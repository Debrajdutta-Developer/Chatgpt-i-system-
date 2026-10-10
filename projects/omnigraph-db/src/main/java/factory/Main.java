package factory;

import java.util.List;

public class Main {
    public static void main(String[] args) {
        System.out.println("Starting OmniGraph DB Server Engine...");
        Core.GraphDatabase db = new Core.GraphDatabase();
        
        Core.Node n1 = db.createNode("User");
        Core.Node n2 = db.createNode("User");
        Core.Edge e1 = db.createEdge(n1.getId(), n2.getId(), "FOLLOWS");
        
        System.out.println("Created Node 1 ID: " + n1.getId());
        System.out.println("Created Node 2 ID: " + n2.getId());
        System.out.println("Created Edge ID: " + e1.getId() + " type: " + e1.getType());
        
        String query = "CREATE (u:User {name:Alice})";
        Core.CypherParser.AST ast = Core.CypherParser.parse(query);
        List<String> plan = Core.QueryPlanner.plan(ast);
        
        System.out.println("Parsed Cypher Command: " + ast.getCommand() + " with label: " + ast.getLabel());
        System.out.println("Query Execution Plan Steps: " + plan.size());
        
        System.out.println("OmniGraph DB initialized and operational.");
    }
}
