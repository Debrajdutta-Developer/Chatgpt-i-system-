package factory;

import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.locks.ReentrantLock;

public class Core {

    public enum TokenType {
        SELECT, FROM, WHERE, INSERT, INTO, VALUES, UPDATE, SET, DELETE, CREATE, TABLE,
        IDENTIFIER, NUMBER_LITERAL, STRING_LITERAL, EQUALS, LESS_THAN, GREATER_THAN,
        COMMA, SEMICOLON, LPAREN, RPAREN, EOF
    }

    public static class Token {
        public final TokenType type;
        public final String lexeme;
        public final int position;

        public Token(TokenType type, String lexeme, int position) {
            this.type = type;
            this.lexeme = lexeme;
            this.position = position;
        }

        @Override
        public String toString() {
            return "Token(" + type + ", \"" + lexeme + "\")";
        }
    }

    public static class Lexer {
        private final String source;
        private int index = 0;

        public Lexer(String source) {
            this.source = source;
        }

        public List<Token> tokenize() {
            List<Token> tokens = new ArrayList<>();
            while (index < source.length()) {
                char c = source.charAt(index);
                if (Character.isWhitespace(c)) {
                    index++;
                } else if (c == ',') {
                    tokens.add(new Token(TokenType.COMMA, ",", index++));
                } else if (c == ';') {
                    tokens.add(new Token(TokenType.SEMICOLON, ";", index++));
                } else if (c == '(') {
                    tokens.add(new Token(TokenType.LPAREN, "(", index++));
                } else if (c == ')') {
                    tokens.add(new Token(TokenType.RPAREN, ")", index++));
                } else if (c == '=') {
                    tokens.add(new Token(TokenType.EQUALS, "=", index++));
                } else if (c == '<') {
                    tokens.add(new Token(TokenType.LESS_THAN, "<", index++));
                } else if (c == '>') {
                    tokens.add(new Token(TokenType.GREATER_THAN, ">", index++));
                } else if (c == '\'') {
                    int start = ++index;
                    while (index < source.length() && source.charAt(index) != '\'') {
                        index++;
                    }
                    String val = source.substring(start, index);
                    if (index < source.length()) index++;
                    tokens.add(new Token(TokenType.STRING_LITERAL, val, start));
                } else if (Character.isDigit(c)) {
                    int start = index;
                    while (index < source.length() && Character.isDigit(source.charAt(index))) {
                        index++;
                    }
                    tokens.add(new Token(TokenType.NUMBER_LITERAL, source.substring(start, index), start));
                } else if (Character.isLetter(c) || c == '_') {
                    int start = index;
                    while (index < source.length() && (Character.isLetterOrDigit(source.charAt(index)) || source.charAt(index) == '_')) {
                        index++;
                    }
                    String lexeme = source.substring(start, index);
                    TokenType type = switch (lexeme.toUpperCase()) {
                        case "SELECT" -> TokenType.SELECT;
                        case "FROM" -> TokenType.FROM;
                        case "WHERE" -> TokenType.WHERE;
                        case "INSERT" -> TokenType.INSERT;
                        case "INTO" -> TokenType.INTO;
                        case "VALUES" -> TokenType.VALUES;
                        case "UPDATE" -> TokenType.UPDATE;
                        case "SET" -> TokenType.SET;
                        case "DELETE" -> TokenType.DELETE;
                        case "CREATE" -> TokenType.CREATE;
                        case "TABLE" -> TokenType.TABLE;
                        default -> TokenType.IDENTIFIER;
                    };
                    tokens.add(new Token(type, lexeme, start));
                } else {
                    throw new RuntimeException("Unexpected character: " + c + " at position " + index);
                }
            }
            tokens.add(new Token(TokenType.EOF, "", index));
            return tokens;
        }
    }

    public interface ASTNode {}

    public static class SelectStatement implements ASTNode {
        public final String tableName;
        public final List<String> projections;
        public final String whereColumn;
        public final String whereOperator;
        public final String whereValue;

        public SelectStatement(String tableName, List<String> projections, String whereColumn, String whereOperator, String whereValue) {
            this.tableName = tableName;
            this.projections = projections;
            this.whereColumn = whereColumn;
            this.whereOperator = whereOperator;
            this.whereValue = whereValue;
        }
    }

    public static class InsertStatement implements ASTNode {
        public final String tableName;
        public final List<String> values;

        public InsertStatement(String tableName, List<String> values) {
            this.tableName = tableName;
            this.values = values;
        }
    }

    public static class Parser {
        private final List<Token> tokens;
        private int current = 0;

        public Parser(List<Token> tokens) {
            this.tokens = tokens;
        }

        private Token peek() {
            return tokens.get(current);
        }

        private Token advance() {
            if (!isAtEnd()) current++;
            return tokens.get(current - 1);
        }

        private boolean isAtEnd() {
            return peek().type == TokenType.EOF;
        }

        private Token consume(TokenType type, String message) {
            if (peek().type == type) return advance();
            throw new RuntimeException(message + " got " + peek());
        }

        public ASTNode parse() {
            Token t = peek();
            if (t.type == TokenType.SELECT) {
                return parseSelect();
            } else if (t.type == TokenType.INSERT) {
                return parseInsert();
            }
            throw new RuntimeException("Unsupported statement: " + t);
        }

        private SelectStatement parseSelect() {
            consume(TokenType.SELECT, "Expected SELECT");
            List<String> projections = new ArrayList<>();
            projections.add(consume(TokenType.IDENTIFIER, "Expected projection column").lexeme);
            while (peek().type == TokenType.COMMA) {
                advance();
                projections.add(consume(TokenType.IDENTIFIER, "Expected projection column").lexeme);
            }
            consume(TokenType.FROM, "Expected FROM");
            String tableName = consume(TokenType.IDENTIFIER, "Expected table name").lexeme;

            String wCol = null, wOp = null, wVal = null;
            if (peek().type == TokenType.WHERE) {
                advance();
                wCol = consume(TokenType.IDENTIFIER, "Expected WHERE column").lexeme;
                Token op = advance();
                if (op.type != TokenType.EQUALS && op.type != TokenType.LESS_THAN && op.type != TokenType.GREATER_THAN) {
                    throw new RuntimeException("Expected comparison operator in WHERE");
                }
                wOp = op.lexeme;
                Token valTok = advance();
                wVal = valTok.lexeme;
            }
            if (peek().type == TokenType.SEMICOLON) {
                advance();
            }
            return new SelectStatement(tableName, projections, wCol, wOp, wVal);
        }

        private InsertStatement parseInsert() {
            consume(TokenType.INSERT, "Expected INSERT");
            consume(TokenType.INTO, "Expected INTO");
            String tableName = consume(TokenType.IDENTIFIER, "Expected table name").lexeme;
            consume(TokenType.LPAREN, "Expected (");
            List<String> values = new ArrayList<>();
            values.add(consume(TokenType.STRING_LITERAL, TokenType.NUMBER_LITERAL, "Expected literal value").lexeme);
            while (peek().type == TokenType.COMMA) {
                advance();
                values.add(consume(TokenType.STRING_LITERAL, TokenType.NUMBER_LITERAL, "Expected literal value").lexeme);
            }
            consume(TokenType.RPAREN, "Expected )");
            if (peek().type == TokenType.SEMICOLON) {
                advance();
            }
            return new InsertStatement(tableName, values);
        }

        private Token consume(TokenType t1, TokenType t2, String msg) {
            Token p = peek();
            if (p.type == t1 || p.type == t2) return advance();
            throw new RuntimeException(msg);
        }
    }

    public static class SlottedPage {
        public static final int PAGE_SIZE = 4096;
        private final byte[] data = new byte[PAGE_SIZE];

        public SlottedPage() {
            writeHeader(0, PAGE_SIZE, 0);
        }

        public SlottedPage(byte[] sourceData) {
            System.arraycopy(sourceData, 0, data, 0, PAGE_SIZE);
        }

        public byte[] getData() {
            return data;
        }

        private void writeHeader(int pageId, int freeSpacePointer, int itemCount) {
            ByteBuffer.wrap(data, 0, 12)
                    .putInt(pageId)
                    .putInt(freeSpacePointer)
                    .putInt(itemCount);
        }

        public int getPageId() {
            return ByteBuffer.wrap(data, 0, 4).getInt();
        }

        public int getFreeSpacePointer() {
            return ByteBuffer.wrap(data, 4, 4).getInt();
        }

        public int getItemCount() {
            return ByteBuffer.wrap(data, 8, 4).getInt();
        }

        public synchronized boolean insertRecord(byte[] recordBytes) {
            int itemCount = getItemCount();
            int freeSpace = getFreeSpacePointer();
            int slotArrayEnd = 12 + (itemCount * 8);
            int requiredSpace = 8 + recordBytes.length;

            if (freeSpace - requiredSpace < slotArrayEnd) {
                return false;
            }

            int newFreeSpace = freeSpace - recordBytes.length;
            System.arraycopy(recordBytes, 0, data, newFreeSpace, recordBytes.length);

            int slotOffset = 12 + (itemCount * 8);
            ByteBuffer.wrap(data, slotOffset, 4).putInt(newFreeSpace);
            ByteBuffer.wrap(data, slotOffset + 4, 4).putInt(recordBytes.length);

            writeHeader(getPageId(), newFreeSpace, itemCount + 1);
            return true;
        }

        public byte[] getRecord(int index) {
            int itemCount = getItemCount();
            if (index < 0 || index >= itemCount) {
                throw new IllegalArgumentException("Invalid record index: " + index);
            }
            int slotOffset = 12 + (index * 8);
            int offset = ByteBuffer.wrap(data, slotOffset, 4).getInt();
            int length = ByteBuffer.wrap(data, slotOffset + 4, 4).getInt();
            byte[] record = new byte[length];
            System.arraycopy(data, offset, record, 0, length);
            return record;
        }
    }

    public static class PageCache {
        private final int capacity;
        private final ConcurrentHashMap<Integer, SlottedPage> cache = new ConcurrentHashMap<>();
        private final ConcurrentHashMap<Integer, ReentrantLock> locks = new ConcurrentHashMap<>();

        public PageCache(int capacity) {
            this.capacity = capacity;
        }

        public SlottedPage getPage(int pageId) {
            return cache.computeIfAbsent(pageId, id -> {
                if (cache.size() >= capacity) {
                    Integer firstKey = cache.keySet().iterator().next();
                    if (firstKey != null) {
                        cache.remove(firstKey);
                    }
                }
                return new SlottedPage();
            });
        }

        public void putPage(int pageId, SlottedPage page) {
            cache.put(pageId, page);
        }

        public int size() {
            return cache.size();
        }
    }

    public static class WriteAheadLog {
        private final List<String> logEntries = new ArrayList<>();

        public synchronized void append(String entry) {
            logEntries.add(entry);
        }

        public synchronized List<String> getEntries() {
            return new ArrayList<>(logEntries);
        }

        public synchronized void clear() {
            logEntries.clear();
        }
    }

    public static class ColumnHistogram {
        public final String columnName;
        public final long totalCount;
        private final Map<String, Long> frequencies = new HashMap<>();

        public ColumnHistogram(String columnName, long totalCount) {
            this.columnName = columnName;
            this.totalCount = totalCount;
        }

        public void addBucket(String value, long frequency) {
            frequencies.put(value, frequency);
        }

        public double estimateSelectivity(String operator, String targetValue) {
            long freq = frequencies.getOrDefault(targetValue, 1L);
            if (totalCount == 0) return 1.0;
            if (operator.equals("=")) {
                return (double) freq / totalCount;
            } else if (operator.equals("<") || operator.equals(">")) {
                return 0.33;
            }
            return 0.1;
        }
    }

    public static class Catalog {
        private final Map<String, List<String>> schemas = new ConcurrentHashMap<>();
        private final Map<String, ColumnHistogram> histograms = new ConcurrentHashMap<>();

        public void registerTable(String tableName, List<String> columns) {
            schemas.put(tableName.toLowerCase(), columns);
        }

        public List<String> getTableSchema(String tableName) {
            return schemas.get(tableName.toLowerCase());
        }

        public void registerHistogram(String key, ColumnHistogram histogram) {
            histograms.put(key.toLowerCase(), histogram);
        }

        public ColumnHistogram getHistogram(String key) {
            return histograms.get(key.toLowerCase());
        }
    }

    public static class CostBasedOptimizer {
        private final Catalog catalog;

        public CostBasedOptimizer(Catalog catalog) {
            this.catalog = catalog;
        }

        public String optimize(SelectStatement stmt) {
            List<String> schema = catalog.getTableSchema(stmt.tableName);
            if (schema == null) {
                return "Plan: SequentialScan(Table=" + stmt.tableName + ") [Cost: 1000.0]";
            }
            if (stmt.whereColumn != null) {
                ColumnHistogram hist = catalog.getHistogram(stmt.tableName + "." + stmt.whereColumn);
                double selectivity = hist != null ? hist.estimateSelectivity(stmt.whereOperator, stmt.whereValue) : 0.1;
                double cost = 1000.0 * selectivity;
                return "Plan: IndexScan(Table=" + stmt.tableName + ", Filter=" + stmt.whereColumn + " " + stmt.whereOperator + " " + stmt.whereValue + ") [Cost: " + cost + "]";
            } else {
                return "Plan: SequentialScan(Table=" + stmt.tableName + ") [Cost: 500.0]";
            }
        }
    }

    public static class VectorBatch {
        public static final int VECTOR_SIZE = 1024;
        public final int[] intData = new int[VECTOR_SIZE];
        public final String[] stringData = new String[VECTOR_SIZE];
        public int size = 0;
    }

    public static class MVCCManager {
        private final AtomicInteger txCounter = new AtomicInteger(1);
        private final ConcurrentHashMap<String, Long> txnTimestamps = new ConcurrentHashMap<>();

        public int beginTransaction() {
            int txId = txCounter.incrementAndGet();
            txnTimestamps.put("tx-" + txId, System.currentTimeMillis());
            return txId;
        }

        public boolean isVisible(int recordCreatedTx, int currentTxId) {
            return recordCreatedTx <= currentTxId;
        }
    }
}
