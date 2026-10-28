package factory;

import java.util.List;

public class IntegrationTest {
    private static int checkCount = 0;

    private static void check(boolean condition, String message) {
        if (!condition) {
            throw new AssertionError("INTEGRATION TEST FAILED: " + message);
        }
        checkCount++;
        System.out.println("  [PASS] " + message);
    }

    public static void main(String[] args) {
        System.out.println("Running IntegrationTest Suite...");

        // Test 1: Complex JOIN query analysis
        Core.SqlParser parser = new Core.SqlParser();
        Core.SqlNode joinNode = parser.parse("SELECT * FROM users JOIN orders ON users.id = orders.user_id");
        check(joinNode != null, "Parser successfully parses JOIN query");

        Core.LogicalAnalyzer analyzer = new Core.LogicalAnalyzer();
        Core.RelNode joinRel = analyzer.analyze(joinNode);
        check(joinRel.type == Core.RelType.INNER_JOIN, "Analyzer constructs INNER_JOIN relational root node");

        // Test 2: Join child relations count and types
        check(joinRel.children.size() == 2, "Inner join has exactly two child relations");
        check(joinRel.children.get(0).type == Core.RelType.TABLE_SCAN, "Left child of join is table scan");
        check(joinRel.children.get(1).type == Core.RelType.TABLE_SCAN, "Right child of join is table scan");

        // Test 3: Catalog statistics configuration for multiple tables
        Core.CatalogStats stats = new Core.CatalogStats();
        stats.setTableRowCount("users", 10000L);
        stats.setTableRowCount("orders", 50000L);
        check(stats.getTableRowCount("users") == 10000L, "Users table row count correctly configured");
        check(stats.getTableRowCount("orders") == 50000L, "Orders table row count correctly configured");

        // Test 4: End-to-end optimization of JOIN query
        Core.Optimizer optimizer = new Core.Optimizer();
        Core.Group optimizedJoinGroup = optimizer.optimize(joinRel, stats);
        check(optimizedJoinGroup.bestExpression != null, "Join query optimization successfully selects best expression");

        // Test 5: Hash join physical rule application check
        boolean hasHashJoin = false;
        for (Core.GroupExpr expr : optimizedJoinGroup.expressions) {
            if (expr.type == Core.RelType.HASH_JOIN) {
                hasHashJoin = true;
                break;
            }
        }
        check(hasHashJoin, "Rule engine successfully generates HASH_JOIN alternative expression");

        // Test 6: Verification harness robust handling of malformed expressions
        Core.VerificationHarness harness = new Core.VerificationHarness();
        boolean robustMalformed = harness.verifyRobustness("SELECT FROM WHERE", stats);
        check(robustMalformed, "Verification harness successfully recovers from malformed SQL input");

        // Test 7: Verification harness handling of zero-cardinality statistics
        Core.CatalogStats zeroStats = new Core.CatalogStats();
        zeroStats.setTableRowCount("users", 0L);
        boolean robustZeroCard = harness.verifyRobustness("SELECT name FROM users WHERE age > 50", zeroStats);
        check(robustZeroCard, "Verification harness successfully handles zero-cardinality catalog stats");

        // Test 8: Plan visualizer explain string structure
        String explainOutput = joinRel.explain(0);
        check(explainOutput.contains("INNER_JOIN"), "Explain output contains INNER_JOIN operator label");
        check(explainOutput.contains("TABLE_SCAN"), "Explain output contains TABLE_SCAN operator label");

        System.out.println("IntegrationTest passed successfully with " + checkCount + " assertions.");
    }
}
