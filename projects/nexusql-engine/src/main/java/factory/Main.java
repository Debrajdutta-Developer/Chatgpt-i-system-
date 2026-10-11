package factory;

import java.util.List;

public class Main {
    public static void main(String[] args) {
        System.out.println("Starting NexusQL Engine Enterprise CLI...");

        Core.Catalog catalog = new Core.Catalog();
        catalog.registerTable("users", List.of("id", "name", "status"));

        Core.ColumnHistogram hist = new Core.ColumnHistogram("status", 100);
        hist.addBucket("active", 80);
        hist.addBucket("inactive", 20);
        catalog.registerHistogram("users.status", hist);

        String sql = "SELECT id, name FROM users WHERE status = 'active';";
        System.out.println("Executing Query: " + sql);

        Core.Lexer lexer = new Core.Lexer(sql);
        List<Core.Token> tokens = lexer.tokenize();
        System.out.println("Lexer produced tokens: " + tokens.size());

        Core.Parser parser = new Core.Parser(tokens);
        Core.ASTNode ast = parser.parse();
        System.out.println("Parser produced AST node: " + ast.getClass().getSimpleName());

        if (ast instanceof Core.SelectStatement select) {
            Core.CostBasedOptimizer cbo = new Core.CostBasedOptimizer(catalog);
            String plan = cbo.optimize(select);
            System.out.println("Optimizer Plan: " + plan);
        }

        Core.SlottedPage page = new Core.SlottedPage();
        boolean inserted = page.insertRecord("user_record_data_sample_1".getBytes());
        System.out.println("Slotted page record insertion status: " + inserted);
        System.out.println("Slotted page item count: " + page.getItemCount());

        System.out.println("NexusQL Engine initialized successfully.");
    }
}
