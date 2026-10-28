package factory;

import java.util.List;

public class Main {
    public static void main(String[] args) {
        String query = (args.length > 0) ? args[0] : "SELECT id, name FROM users WHERE age > 25";
        System.out.println("=== OmniQuery Planner CLI ===");
        System.out.println("Input SQL: " + query);

        Core.CatalogStats stats = new Core.CatalogStats();
        stats.setTableRowCount("users", 5000L);
        stats.setColumnRange("users", "age", 18.0, 99.0);

        Core.SqlParser parser = new Core.SqlParser();
        Core.SqlNode sqlNode = parser.parse(query);

        Core.LogicalAnalyzer analyzer = new Core.LogicalAnalyzer();
        Core.RelNode relRoot = analyzer.analyze(sqlNode);

        System.out.println("\n--- Initial Logical Plan ---");
        System.out.print(relRoot.explain(0));

        Core.Optimizer optimizer = new Core.Optimizer();
        Core.Group bestGroup = optimizer.optimize(relRoot, stats);

        System.out.println("\n--- Optimization Complete ---");
        System.out.println("Best Group Lowest Cost: " + bestGroup.lowestCost);
        if (bestGroup.bestExpression != null) {
            System.out.println("Best Selected Operator: " + bestGroup.bestExpression.type);
        }
        System.out.println("OmniQuery Planner executed successfully.");
    }
}
