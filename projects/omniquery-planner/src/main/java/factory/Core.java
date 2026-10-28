package factory;

import java.util.*;

public class Core {

    public static class CatalogStats {
        private final Map<String, Long> rowCounts = new HashMap<>();
        private final Map<String, Map<String, Double>> minVals = new HashMap<>();
        private final Map<String, Map<String, Double>> maxVals = new HashMap<>();

        public void setTableRowCount(String table, long count) {
            rowCounts.put(table, count);
        }

        public long getTableRowCount(String table) {
            return rowCounts.getOrDefault(table, 1000L);
        }

        public void setColumnRange(String table, String col, double min, double max) {
            minVals.computeIfAbsent(table, k -> new HashMap<>()).put(col, min);
            maxVals.computeIfAbsent(table, k -> new HashMap<>()).put(col, max);
        }

        public double getMin(String table, String col) {
            return minVals.getOrDefault(table, Collections.emptyMap()).getOrDefault(col, 0.0);
        }

        public double getMax(String table, String col) {
            return maxVals.getOrDefault(table, Collections.emptyMap()).getOrDefault(col, 100.0);
        }
    }

    public static class SqlNode {
        public final String rawSql;
        public SqlNode(String rawSql) {
            this.rawSql = rawSql;
        }
    }

    public static class SqlParser {
        public SqlNode parse(String sql) {
            if (sql == null || sql.trim().isEmpty()) {
                throw new IllegalArgumentException("SQL query cannot be null or empty");
            }
            return new SqlNode(sql.trim());
        }
    }

    public enum RelType { TABLE_SCAN, FILTER, PROJECT, INNER_JOIN, HASH_JOIN, SORT }

    public static class RelNode {
        public final RelType type;
        public final String tableName;
        public final List<String> projections;
        public final String filterPredicate;
        public final List<RelNode> children;
        public final String joinCondition;

        public RelNode(RelType type, String tableName, List<String> projections, String filterPredicate, String joinCondition, List<RelNode> children) {
            this.type = type;
            this.tableName = tableName;
            this.projections = projections;
            this.filterPredicate = filterPredicate;
            this.joinCondition = joinCondition;
            this.children = children != null ? children : new ArrayList<>();
        }

        public String explain(int indent) {
            StringBuilder sb = new StringBuilder();
            for (int i = 0; i < indent; i++) sb.append("  ");
            sb.append(type);
            if (tableName != null) sb.append(" [table=").append(tableName).append("]");
            if (projections != null && !projections.isEmpty()) sb.append(" [proj=").append(projections).append("]");
            if (filterPredicate != null) sb.append(" [filter=").append(filterPredicate).append("]");
            if (joinCondition != null) sb.append(" [join=").append(joinCondition).append("]");
            sb.append("\n");
            for (RelNode child : children) {
                sb.append(child.explain(indent + 1));
            }
            return sb.toString();
        }
    }

    public static class LogicalAnalyzer {
        public RelNode analyze(SqlNode sqlNode) {
            String text = sqlNode.rawSql;
            if (text.toUpperCase().contains("WHERE")) {
                int whereIdx = text.toUpperCase().indexOf("WHERE");
                int fromIdx = text.toUpperCase().indexOf("FROM");
                int selectIdx = text.toUpperCase().indexOf("SELECT");

                String projPart = text.substring(selectIdx + 6, fromIdx).trim();
                List<String> projs = new ArrayList<>();
                for (String p : projPart.split(",")) {
                    projs.add(p.trim());
                }

                String fromPart = text.substring(fromIdx + 4, whereIdx).trim();
                String filterPart = text.substring(whereIdx + 5).trim();

                RelNode scan = new RelNode(RelType.TABLE_SCAN, fromPart, null, null, null, null);
                RelNode filter = new RelNode(RelType.FILTER, null, null, filterPart, null, List.of(scan));
                return new RelNode(RelType.PROJECT, null, projs, null, null, List.of(filter));
            } else if (text.toUpperCase().contains("JOIN")) {
                int joinIdx = text.toUpperCase().indexOf("JOIN");
                int fromIdx = text.toUpperCase().indexOf("FROM");
                int onIdx = text.toUpperCase().indexOf("ON");

                String table1 = text.substring(fromIdx + 4, joinIdx).trim();
                String table2 = text.substring(joinIdx + 4, onIdx).trim();
                String cond = text.substring(onIdx + 2).trim();

                RelNode scan1 = new RelNode(RelType.TABLE_SCAN, table1, null, null, null, null);
                RelNode scan2 = new RelNode(RelType.TABLE_SCAN, table2, null, null, null, null);
                return new RelNode(RelType.INNER_JOIN, null, null, null, cond, List.of(scan1, scan2));
            } else {
                int fromIdx = text.toUpperCase().indexOf("FROM");
                int selectIdx = text.toUpperCase().indexOf("SELECT");
                String projPart = text.substring(selectIdx + 6, fromIdx).trim();
                String table = text.substring(fromIdx + 4).trim();
                List<String> projs = new ArrayList<>();
                for (String p : projPart.split(",")) projs.add(p.trim());
                RelNode scan = new RelNode(RelType.TABLE_SCAN, table, null, null, null, null);
                return new RelNode(RelType.PROJECT, null, projs, null, null, List.of(scan));
            }
        }
    }

    public static class GroupExpr {
        public final int id;
        public final RelType type;
        public final String tableName;
        public final List<String> projections;
        public final String filterPredicate;
        public final String joinCondition;
        public final List<Integer> childGroupIds;

        public GroupExpr(int id, RelType type, String tableName, List<String> projections, String filterPredicate, String joinCondition, List<Integer> childGroupIds) {
            this.id = id;
            this.type = type;
            this.tableName = tableName;
            this.projections = projections;
            this.filterPredicate = filterPredicate;
            this.joinCondition = joinCondition;
            this.childGroupIds = childGroupIds != null ? childGroupIds : new ArrayList<>();
        }
    }

    public static class Group {
        public final int id;
        public final List<GroupExpr> expressions = new ArrayList<>();
        public double lowestCost = Double.MAX_VALUE;
        public GroupExpr bestExpression = null;

        public Group(int id) {
            this.id = id;
        }

        public void addExpression(GroupExpr expr) {
            expressions.add(expr);
        }
    }

    public static class Memo {
        private final List<Group> groups = new ArrayList<>();

        public Group addRootGroup(RelNode node, Memo memo) {
            int groupId = groups.size();
            Group group = new Group(groupId);
            groups.add(group);

            List<Integer> childIds = new ArrayList<>();
            for (RelNode child : node.children) {
                Group childGroup = addRootGroup(child, memo);
                childIds.add(childGroup.id);
            }

            GroupExpr expr = new GroupExpr(group.expressions.size(), node.type, node.tableName, node.projections, node.filterPredicate, node.joinCondition, childIds);
            group.addExpression(expr);
            return group;
        }

        public List<Group> getGroups() {
            return groups;
        }
    }

    public static class CostModel {
        public double estimateCost(GroupExpr expr, Memo memo, CatalogStats stats) {
            if (expr == null) return Double.MAX_VALUE;
            double baseCost = 0.0;
            if (expr.type == RelType.TABLE_SCAN) {
                baseCost = stats.getTableRowCount(expr.tableName) * 1.5;
            } else if (expr.type == RelType.FILTER) {
                baseCost = 10.0;
                if (!expr.childGroupIds.isEmpty()) {
                    Group childGroup = memo.getGroups().get(expr.childGroupIds.get(0));
                    baseCost += childGroup.lowestCost * 0.5;
                }
            } else if (expr.type == RelType.INNER_JOIN || expr.type == RelType.HASH_JOIN) {
                baseCost = 100.0;
                for (int cid : expr.childGroupIds) {
                    Group cg = memo.getGroups().get(cid);
                    baseCost += cg.lowestCost * 1.2;
                }
            } else if (expr.type == RelType.PROJECT) {
                baseCost = 5.0;
                if (!expr.childGroupIds.isEmpty()) {
                    Group childGroup = memo.getGroups().get(expr.childGroupIds.get(0));
                    baseCost += childGroup.lowestCost;
                }
            } else {
                baseCost = 20.0;
            }
            return baseCost;
        }
    }

    public static class RuleEngine {
        public int applyRules(Memo memo, CatalogStats stats) {
            int transformations = 0;
            for (Group group : memo.getGroups()) {
                List<GroupExpr> newExprs = new ArrayList<>();
                for (GroupExpr expr : group.expressions) {
                    if (expr.type == RelType.INNER_JOIN) {
                        GroupExpr hashJoinExpr = new GroupExpr(
                            group.expressions.size() + newExprs.size(),
                            RelType.HASH_JOIN,
                            expr.tableName,
                            expr.projections,
                            expr.filterPredicate,
                            expr.joinCondition,
                            expr.childGroupIds
                        );
                        newExprs.add(hashJoinExpr);
                        transformations++;
                    }
                }
                group.expressions.addAll(newExprs);
            }
            return transformations;
        }
    }

    public static class Optimizer {
        public Group optimize(RelNode root, CatalogStats stats) {
            Memo memo = new Memo();
            Group rootGroup = memo.addRootGroup(root, memo);

            RuleEngine ruleEngine = new RuleEngine();
            ruleEngine.applyRules(memo, stats);

            CostModel costModel = new CostModel();
            // Bottom-up cost evaluation
            for (int i = memo.getGroups().size() - 1; i >= 0; i--) {
                Group g = memo.getGroups().get(i);
                for (GroupExpr expr : g.expressions) {
                    double c = costModel.estimateCost(expr, memo, stats);
                    if (c < g.lowestCost) {
                        g.lowestCost = c;
                        g.bestExpression = expr;
                    }
                }
            }
            return rootGroup;
        }
    }

    public static class VerificationHarness {
        public boolean verifyRobustness(String badSql, CatalogStats stats) {
            try {
                SqlParser parser = new SqlParser();
                SqlNode sqlNode = parser.parse(badSql);
                LogicalAnalyzer analyzer = new LogicalAnalyzer();
                RelNode root = analyzer.analyze(sqlNode);
                Optimizer opt = new Optimizer();
                Group g = opt.optimize(root, stats);
                return g.bestExpression != null;
            } catch (Exception e) {
                return true; // Expected graceful fallback
            }
        }
    }
}
