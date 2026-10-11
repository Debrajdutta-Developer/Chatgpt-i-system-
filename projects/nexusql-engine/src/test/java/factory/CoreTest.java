package factory;

import java.util.List;

public class CoreTest {

    private static void check(boolean condition, String message) {
        if (!condition) {
            throw new AssertionError("TEST FAILURE: " + message);
        }
        System.out.println("[PASS] " + message);
    }

    public static void main(String[] args) {
        System.out.println("Running CoreTest Suite...");

        // 1. Lexer tests
        Core.Lexer lexer = new Core.Lexer("SELECT id FROM users WHERE id = 1;");
        List<Core.Token> tokens = lexer.tokenize();
        check(tokens.size() > 0, "assertLexerTokenCountNonZero");
        check(tokens.get(0).type == Core.TokenType.SELECT, "assertLexerFirstTokenSelect");

        // 2. Parser tests
        Core.Parser parser = new Core.Parser(tokens);
        Core.ASTNode ast = parser.parse();
        check(ast instanceof Core.SelectStatement, "assertParserProducesSelectStmt");
        Core.SelectStatement select = (Core.SelectStatement) ast;
        check(select.tableName.equals("users"), "assertParserTableNameCorrect");
        check(select.whereColumn.equals("id"), "assertParserWhereColumnCorrect");
        check(select.whereValue.equals("1"), "assertParserWhereValueCorrect");

        // 3. Storage tests
        Core.SlottedPage page = new Core.SlottedPage();
        boolean ins1 = page.insertRecord("record_one".getBytes());
        boolean ins2 = page.insertRecord("record_two".getBytes());
        check(ins1, "assertSlottedPageInsertFirstRecord");
        check(ins2, "assertSlottedPageInsertSecondRecord");
        check(page.getItemCount() == 2, "assertSlottedPageItemCountTwo");
        String rec0 = new String(page.getRecord(0));
        check(rec0.equals("record_one"), "assertSlottedPageRecordZeroContent");

        // 4. Cache tests
        Core.PageCache cache = new Core.PageCache(2);
        Core.SlottedPage p1 = cache.getPage(100);
        cache.putPage(100, p1);
        check(cache.size() == 1, "assertPageCacheSizeOne");

        // 5. MVCC tests
        Core.MVCCManager mvcc = new Core.MVCCManager();
        int tx1 = mvcc.beginTransaction();
        int tx2 = mvcc.beginTransaction();
        check(tx2 > tx1, "assertMVCCMonotonicTransactions");
        check(mvcc.isVisible(1, tx1), "assertMVCCVisibilityTrue");

        System.out.println("CoreTest Suite completed successfully with all assertions verified.");
    }
}
