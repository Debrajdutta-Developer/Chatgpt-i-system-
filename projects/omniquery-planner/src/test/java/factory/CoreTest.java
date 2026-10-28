package factory;

import java.util.List;

public class CoreTest {
    private static int checkCount = 0;

    private static void check(boolean condition, String message) {
        if (!condition) {
            throw new AssertionError("TEST FAILED: " + message);
        }
        checkCount++;
        System.out.println("  [PASS] " + message);
    }

    public static void main(String[] args) {
        System.out.println("Running CoreTest Suite...");

        // Test 1: Parser valid SQL
        Core.SqlParser parser = new Core.SqlParser();
        Core.SqlNode node = parser.parse("SELECT a, b FROM t1 WHERE a > 10");
        check(node != null, "Parser returns valid SqlNode for normal query");

        // Test 2: Parser malformed/empty SQL exception handling
        boolean threw = false;
        try {
            parser.parse("");
        } catch (IllegalArgumentException e) {
            threw = true;
        }
        check(threw, "Parser throws IllegalArgumentException on empty query");

        // Test 3: Logical Analyzer selection query
        Core.LogicalAnalyzer analyzer = new Core.LogicalAnalyzer();
        Core.RelNode rel1 = analyzer.analyze(node);
        check(rel1.type == Core.RelType.PROJECT, "Analyzer produces PROJECT root for SELECT query");

        // Test 4: Logical Analyzer filter child node
        check(rel1.children.get(0).type == Core.RelType.FILTER, "Analyzer produces FILTER child under PROJECT");

        // Test 5: Logical Analyzer table scan node
        check(rel1.children.get(0).children.get(0).type == Core.RelType.TABLE_SCAN, "Analyzer produces TABLE_SCAN under FILTER");

        // Test 6: Catalog Statistics population and retrieval
        Core.CatalogStats stats = new Core.CatalogStats();
        stats.setTableRowCount("t1", 1234L);
        check(stats.getTableRowCount("t1") == 1234L, "Catalog statistics correctly stores row count");

        // Test 7: Catalog Statistics default fallback count
        check(stats.getTableRowCount("unknown_table") == 1000L, "Catalog statistics returns default fallback for unknown table");

        // Test 8: Rule Engine rule transformation count
        Core.Memo memo = new Core.Memo();
        Core.Group g = memo.addRootGroup(rel1, memo);
        Core.RuleEngine ruleEngine = new Core.RuleEngine();
        int transformed = ruleEngine.applyRules(memo, stats);
        check(transformed >= 0, "Rule engine executes rule transformations without error");

        // Test 9: Cost Model estimation on Table Scan
        Core.GroupExpr scanExpr = new Core.GroupExpr(0, Core.RelType.TABLE_SCAN, "t1", null, null, null, null);
        Core.CostModel costModel = new Core.CostModel();
        double scanCost = costModel.estimateCost(scanExpr, memo, stats);
        check(scanCost > 0.0, "Cost model computes positive cost for table scan");

        // Test 10: Optimizer produces valid best group
        Core.Optimizer optimizer = new Core.Optimizer();
        Core.Group bestGroup = optimizer.optimize(rel1, stats);
        check(bestGroup.bestExpression != null, "Optimizer selects valid best expression in root group");

        System.out.println("CoreTest passed successfully with " + checkCount + " assertions.");
    }
}
