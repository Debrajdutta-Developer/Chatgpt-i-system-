package factory;

import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.locks.ReentrantReadWriteLock;

public class Core {

    public enum PropertyType { LONG, DOUBLE, STRING, BOOLEAN }

    public static class PropertyValue {
        private final PropertyType type;
        private final long longVal;
        private final double doubleVal;
        private final String stringVal;
        private final boolean boolVal;

        public PropertyValue(long v) { this.type = PropertyType.LONG; this.longVal = v; this.doubleVal = 0; this.stringVal = null; this.boolVal = false; }
        public PropertyValue(double v) { this.type = PropertyType.DOUBLE; this.longVal = 0; this.doubleVal = v; this.stringVal = null; this.boolVal = false; }
        public PropertyValue(String v) { this.type = PropertyType.STRING; this.longVal = 0; this.doubleVal = 0; this.stringVal = v; this.boolVal = false; }
        public PropertyValue(boolean v) { this.type = PropertyType.BOOLEAN; this.longVal = 0; this.doubleVal = 0; this.stringVal = null; this.boolVal = v; }

        public PropertyType getType() { return type; }
        public Object getValue() {
            return switch (type) {
                case LONG -> longVal;
                case DOUBLE -> doubleVal;
                case STRING -> stringVal;
                case BOOLEAN -> boolVal;
            };
        }
    }

    public static class Node {
        private final long id;
        private final String label;
        private final Map<String, PropertyValue> properties;
        private final List<Edge> outgoingEdges;
        private final List<Edge> incomingEdges;

        public Node(long id, String label) {
            this.id = id;
            this.label = label;
            this.properties = new ConcurrentHashMap<>();
            this.outgoingEdges = Collections.synchronizedList(new ArrayList<>());
            this.incomingEdges = Collections.synchronizedList(new ArrayList<>());
        }

        public long getId() { return id; }
        public String getLabel() { return label; }
        public Map<String, PropertyValue> getProperties() { return properties; }
        public List<Edge> getOutgoingEdges() { return outgoingEdges; }
        public List<Edge> getIncomingEdges() { return incomingEdges; }
    }

    public static class Edge {
        private final long id;
        private final long sourceId;
        private final long targetId;
        private final String type;
        private final Map<String, PropertyValue> properties;

        public Edge(long id, long sourceId, long targetId, String type) {
            this.id = id;
            this.sourceId = sourceId;
            this.targetId = targetId;
            this.type = type;
            this.properties = new ConcurrentHashMap<>();
        }

        public long getId() { return id; }
        public long getSourceId() { return sourceId; }
        public long getTargetId() { return targetId; }
        public String getType() { return type; }
        public Map<String, PropertyValue> getProperties() { return properties; }
    }

    public static class GraphDatabase {
        private final Map<Long, Node> nodes = new ConcurrentHashMap<>();
        private final Map<Long, Edge> edges = new ConcurrentHashMap<>();
        private final ReentrantReadWriteLock lock = new ReentrantReadWriteLock();
        private final List<String> journal = Collections.synchronizedList(new ArrayList<>());
        private long nextId = 1;

        public long generateId() {
            lock.writeLock().lock();
            try {
                return nextId++;
            } finally {
                lock.writeLock().unlock();
            }
        }

        public Node createNode(String label) {
            lock.writeLock().lock();
            try {
                long id = generateId();
                Node node = new Node(id, label);
                nodes.put(id, node);
                journal.add("CREATE_NODE:" + id + ":" + label);
                return node;
            } finally {
                lock.writeLock().unlock();
            }
        }

        public Edge createEdge(long sourceId, long targetId, String type) {
            lock.writeLock().lock();
            try {
                Node source = nodes.get(sourceId);
                Node target = nodes.get(targetId);
                if (source == null || target == null) {
                    throw new IllegalArgumentException("Source or target node not found");
                }
                long id = generateId();
                Edge edge = new Edge(id, sourceId, targetId, type);
                edges.put(id, edge);
                source.getOutgoingEdges().add(edge);
                target.getIncomingEdges().add(edge);
                journal.add("CREATE_EDGE:" + id + ":" + sourceId + ":" + targetId + ":" + type);
                return edge;
            } finally {
                lock.writeLock().unlock();
            }
        }

        public boolean deleteEdge(long edgeId) {
            lock.writeLock().lock();
            try {
                Edge edge = edges.remove(edgeId);
                if (edge == null) return false;
                Node source = nodes.get(edge.getSourceId());
                Node target = nodes.get(edge.getTargetId());
                if (source != null) source.getOutgoingEdges().removeIf(e -> e.getId() == edgeId);
                if (target != null) target.getIncomingEdges().removeIf(e -> e.getId() == edgeId);
                journal.add("DELETE_EDGE:" + edgeId);
                return true;
            } finally {
                lock.writeLock().unlock();
            }
        }

        public Node getNode(long id) { return nodes.get(id); }
        public Edge getEdge(long id) { return edges.get(id); }
        public Collection<Node> getAllNodes() { return nodes.values(); }
        public Collection<Edge> getAllEdges() { return edges.values(); }
        public List<String> getJournal() { return journal; }

        public boolean detectCycle() {
            lock.readLock().lock();
            try {
                Set<Long> visited = new HashSet<>();
                Set<Long> recStack = new HashSet<>();
                for (Long nodeId : nodes.keySet()) {
                    if (hasCycleUtil(nodeId, visited, recStack)) {
                        return true;
                    }
                }
                return false;
            } finally {
                lock.readLock().unlock();
            }
        }

        private boolean hasCycleUtil(long nodeId, Set<Long> visited, Set<Long> recStack) {
            if (recStack.contains(nodeId)) return true;
            if (visited.contains(nodeId)) return false;
            visited.add(nodeId);
            recStack.add(nodeId);
            Node node = nodes.get(nodeId);
            if (node != null) {
                for (Edge edge : node.getOutgoingEdges()) {
                    if (hasCycleUtil(edge.getTargetId(), visited, recStack)) {
                        return true;
                    }
                }
            }
            recStack.remove(nodeId);
            return false;
        }

        public List<Long> bfsPath(long startId, long targetId) {
            lock.readLock().lock();
            try {
                Map<Long, Long> parentMap = new HashMap<>();
                Queue<Long> queue = new LinkedList<>();
                Set<Long> visited = new HashSet<>();

                queue.add(startId);
                visited.add(startId);

                boolean found = false;
                while (!queue.isEmpty()) {
                    long curr = queue.poll();
                    if (curr == targetId) {
                        found = true;
                        break;
                    }
                    Node node = nodes.get(curr);
                    if (node != null) {
                        for (Edge edge : node.getOutgoingEdges()) {
                            long next = edge.getTargetId();
                            if (!visited.contains(next)) {
                                visited.add(next);
                                parentMap.put(next, curr);
                                queue.add(next);
                            }
                        }
                    }
                }

                if (!found) return Collections.emptyList();

                List<Long> path = new ArrayList<>();
                long curr = targetId;
                while (curr != startId) {
                    path.add(0, curr);
                    curr = parentMap.get(curr);
                }
                path.add(0, startId);
                return path;
            } finally {
                lock.readLock().unlock();
            }
        }
    }

    public static class CypherParser {
        public static class AST {
            private final String command;
            private final String label;
            private final String variable;
            private final Map<String, String> properties;

            public AST(String command, String label, String variable, Map<String, String> properties) {
                this.command = command;
                this.label = label;
                this.variable = variable;
                this.properties = properties;
            }

            public String getCommand() { return command; }
            public String getLabel() { return label; }
            public String getVariable() { return variable; }
            public Map<String, String> getProperties() { return properties; }
        }

        public static AST parse(String query) {
            if (query == null || query.isBlank()) {
                throw new IllegalArgumentException("Query cannot be blank");
            }
            String trimmed = query.trim();
            if (trimmed.startsWith("CREATE")) {
                int nodeIdx = trimmed.indexOf("(");
                int colonIdx = trimmed.indexOf(":");
                int braceIdx = trimmed.indexOf("{");
                int endParen = trimmed.lastIndexOf(")");

                if (nodeIdx == -1 || colonIdx == -1 || endParen == -1) {
                    throw new IllegalArgumentException("Malformed CREATE query syntax");
                }

                String variable = trimmed.substring(nodeIdx + 1, colonIdx).trim();
                String label;
                if (braceIdx != -1 && braceIdx < endParen) {
                    label = trimmed.substring(colonIdx + 1, braceIdx).trim();
                } else {
                    label = trimmed.substring(colonIdx + 1, endParen).trim();
                }

                Map<String, String> props = new HashMap<>();
                if (braceIdx != -1) {
                    int closeBrace = trimmed.lastIndexOf("}");
                    if (closeBrace == -1) {
                        throw new IllegalArgumentException("Unmatched opening brace in properties");
                    }
                    String propContent = trimmed.substring(braceIdx + 1, closeBrace).trim();
                    if (!propContent.isEmpty()) {
                        String[] pairs = propContent.split(",");
                        for (String pair : pairs) {
                            String[] kv = pair.split(":");
                            if (kv.length != 2) {
                                throw new IllegalArgumentException("Invalid property format: " + pair);
                            }
                            String k = kv[0].trim();
                            String v = kv[1].trim();
                            if (v.endsWith("))")) v = v.substring(0, v.length() - 1);
                            props.put(k, v);
                        }
                    }
                }
                return new AST("CREATE", label, variable, props);
            } else if (trimmed.startsWith("MATCH")) {
                return new AST("MATCH", trimmed, "match", Collections.emptyMap());
            } else {
                throw new IllegalArgumentException("Unsupported command: " + trimmed);
            }
        }
    }

    public static class QueryPlanner {
        public static List<String> plan(CypherParser.AST ast) {
            List<String> planSteps = new ArrayList<>();
            if ("CREATE".equals(ast.getCommand())) {
                planSteps.add("SCAN_CATALOG");
                planSteps.add("ALLOCATE_NODE_ID");
                planSteps.add("WRITE_MUTATION_JOURNAL");
                planSteps.add("COMMIT_NODE_STORE");
            } else if ("MATCH".equals(ast.getCommand())) {
                planSteps.add("INDEX_SCAN");
                planSteps.add("FILTER_PREDICATES");
                planSteps.add("PROJECT_RESULTS");
            }
            return planSteps;
        }
    }

    public static class StorageEngine {
        private final ByteBuffer buffer;

        public StorageEngine(int capacity) {
            this.buffer = ByteBuffer.allocateDirect(capacity);
        }

        public void writeNodeRecord(long id, String label) {
            buffer.putLong(id);
            byte[] labelBytes = label.getBytes(StandardCharsets.UTF_8);
            buffer.putInt(labelBytes.length);
            buffer.put(labelBytes);
        }

        public int capacity() { return buffer.capacity(); }
        public int position() { return buffer.position(); }
    }
}
