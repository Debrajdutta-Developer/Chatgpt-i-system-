import ast
import sys
from typing import Dict, List, Set, Tuple, Optional, Any as PyAny, Union

# --- Type Representation ---

class TypeNode:
    def __repr__(self) -> str:
        raise NotImplementedError

class PrimType(TypeNode):
    def __init__(self, name: str):
        self.name = name
    def __repr__(self) -> str:
        return self.name
    def __eq__(self, other: object) -> bool:
        return isinstance(other, PrimType) and self.name == other.name
    def __hash__(self) -> int:
        return hash(("PrimType", self.name))

class TypeVar(TypeNode):
    def __init__(self, name: str):
        self.name = name
        self.instance: Optional[TypeNode] = None
    def __repr__(self) -> str:
        if self.instance:
            return repr(self.instance)
        return f"~{self.name}"
    def __eq__(self, other: object) -> bool:
        if isinstance(other, TypeVar):
            return self.name == other.name
        return False
    def __hash__(self) -> int:
        return hash(("TypeVar", self.name))

class ListType(TypeNode):
    def __init__(self, element_type: TypeNode):
        self.element_type = element_type
    def __repr__(self) -> str:
        return f"List[{repr(self.element_type)}]"
    def __eq__(self, other: object) -> bool:
        return isinstance(other, ListType) and self.element_type == other.element_type
    def __hash__(self) -> int:
        return hash(("ListType", self.element_type))

class DictType(TypeNode):
    def __init__(self, key_type: TypeNode, value_type: TypeNode):
        self.key_type = key_type
        self.value_type = value_type
    def __repr__(self) -> str:
        return f"Dict[{repr(self.key_type)}, {repr(self.value_type)}]"
    def __eq__(self, other: object) -> bool:
        return isinstance(other, DictType) and self.key_type == other.key_type and self.value_type == other.value_type
    def __hash__(self) -> int:
        return hash(("DictType", self.key_type, self.value_type))

class FuncType(TypeNode):
    def __init__(self, arg_types: List[TypeNode], return_type: TypeNode):
        self.arg_types = arg_types
        self.return_type = return_type
    def __repr__(self) -> str:
        args = ", ".join(repr(t) for t in self.arg_types)
        return f"Callable[[{args}], {repr(self.return_type)}]"
    def __eq__(self, other: object) -> bool:
        return isinstance(other, FuncType) and self.arg_types == other.arg_types and self.return_type == other.return_type
    def __hash__(self) -> int:
        return hash(("FuncType", tuple(self.arg_types), self.return_type))

class ProtocolType(TypeNode):
    def __init__(self, name: str, methods: Dict[str, TypeNode]):
        self.name = name
        self.methods = methods
    def __repr__(self) -> str:
        return f"Protocol[{self.name}]"
    def __eq__(self, other: object) -> bool:
        return isinstance(other, ProtocolType) and self.name == other.name and self.methods == other.methods
    def __hash__(self) -> int:
        return hash(("ProtocolType", self.name))

# --- Abstract Domain ---

class Interval:
    def __init__(self, min_val: Optional[int] = None, max_val: Optional[int] = None):
        self.min_val = min_val
        self.max_val = max_val
    def __repr__(self) -> str:
        return f"[{self.min_val if self.min_val is not None else '-inf'}, {self.max_val if self.max_val is not None else 'inf'}]"
    def join(self, other: 'Interval') -> 'Interval':
        new_min = None if (self.min_val is None or other.min_val is None) else min(self.min_val, other.min_val)
        new_max = None if (self.max_val is None or other.max_val is None) else max(self.max_val, other.max_val)
        return Interval(new_min, new_max)

# --- Control Flow Graph ---

class BasicBlock:
    def __init__(self, block_id: int):
        self.block_id = block_id
        self.statements: List[ast.AST] = []
        self.successors: List['BasicBlock'] = []
        self.predecessors: List['BasicBlock'] = []
        self.jump_true: Optional['BasicBlock'] = None
        self.jump_false: Optional['BasicBlock'] = None
        self.predicate: Optional[ast.AST] = None

class ControlFlowGraph:
    def __init__(self):
        self.blocks: List[BasicBlock] = []
        self.entry_block: Optional[BasicBlock] = None

class CFGBuilder(ast.NodeVisitor):
    def __init__(self):
        self.cfg = ControlFlowGraph()
        self.current_block = self.new_block()
        self.cfg.entry_block = self.current_block

    def new_block(self) -> BasicBlock:
        b = BasicBlock(len(self.cfg.blocks))
        self.cfg.blocks.append(b)
        return b

    def visit_FunctionDef(self, node: ast.FunctionDef) -> None:
        self.current_block.statements.append(node)
        for stmt in node.body:
            self.visit(stmt)

    def visit_Assign(self, node: ast.Assign) -> None:
        self.current_block.statements.append(node)

    def visit_Expr(self, node: ast.Expr) -> None:
        self.current_block.statements.append(node)

    def visit_Return(self, node: ast.Return) -> None:
        self.current_block.statements.append(node)

    def visit_If(self, node: ast.If) -> None:
        self.current_block.statements.append(node)
        self.current_block.predicate = node.test
        
        split_block = self.current_block
        true_block = self.new_block()
        false_block = self.new_block()
        
        split_block.jump_true = true_block
        split_block.jump_false = false_block
        split_block.successors.extend([true_block, false_block])
        true_block.predecessors.append(split_block)
        false_block.predecessors.append(split_block)
        
        # Visit true branch
        self.current_block = true_block
        for stmt in node.body:
            self.visit(stmt)
        end_true = self.current_block
        
        # Visit false branch
        self.current_block = false_block
        for stmt in node.orelse:
            self.visit(stmt)
        end_false = self.current_block
        
        # Join block
        join_block = self.new_block()
        end_true.successors.append(join_block)
        join_block.predecessors.append(end_true)
        end_false.successors.append(join_block)
        join_block.predecessors.append(end_false)
        self.current_block = join_block

    def visit_While(self, node: ast.While) -> None:
        header_block = self.new_block()
        self.current_block.successors.append(header_block)
        header_block.predecessors.append(self.current_block)
        
        self.current_block = header_block
        header_block.predicate = node.test
        
        body_block = self.new_block()
        exit_block = self.new_block()
        
        header_block.jump_true = body_block
        header_block.jump_false = exit_block
        header_block.successors.extend([body_block, exit_block])
        body_block.predecessors.append(header_block)
        exit_block.predecessors.append(header_block)
        
        self.current_block = body_block
        for stmt in node.body:
            self.visit(stmt)
        self.current_block.successors.append(header_block)
        header_block.predecessors.append(self.current_block)
        
        self.current_block = exit_block

# --- Unification & Engine ---

class AnalysisEngine:
    def __init__(self):
        self.errors: List[Dict[str, PyAny]] = []
        self.warnings: List[Dict[str, PyAny]] = []
        self.inferred_types: Dict[str, str] = {}
        self.type_var_counter = 0

    def fresh_var(self) -> TypeVar:
        self.type_var_counter += 1
        return TypeVar(f"t{self.type_var_counter}")

    def unify(self, t1: TypeNode, t2: TypeNode) -> None:
        root1 = self.deref(t1)
        root2 = self.deref(t2)
        
        if isinstance(root1, TypeVar) and isinstance(root2, TypeVar) and root1.name == root2.name:
            return
        if isinstance(root1, TypeVar):
            root1.instance = root2
            return
        if isinstance(root2, TypeVar):
            root2.instance = root1
            return
            
        if isinstance(root1, PrimType) and isinstance(root2, PrimType):
            if root1.name != root2.name:
                raise TypeError(f"Cannot unify {root1} with {root2}")
            return
            
        if isinstance(root1, ListType) and isinstance(root2, ListType):
            self.unify(root1.element_type, root2.element_type)
            return
            
        if isinstance(root1, DictType) and isinstance(root2, DictType):
            self.unify(root1.key_type, root2.key_type)
            self.unify(root1.value_type, root2.value_type)
            return
            
        if isinstance(root1, FuncType) and isinstance(root2, FuncType):
            if len(root1.arg_types) != len(root2.arg_types):
                raise TypeError(f"Callable argument count mismatch: {len(root1.arg_types)} vs {len(root2.arg_types)}")
            for a1, a2 in zip(root1.arg_types, root2.arg_types):
                self.unify(a1, a2)
            self.unify(root1.return_type, root2.return_type)
            return

        raise TypeError(f"Unification failure between {root1} and {root2}")

    def deref(self, t: TypeNode) -> TypeNode:
        curr = t
        while isinstance(curr, TypeVar) and curr.instance is not None:
            curr = curr.instance
        return curr

    def analyze_expr(self, node: ast.AST, env: Dict[str, TypeNode]) -> TypeNode:
        if isinstance(node, ast.Constant):
            if isinstance(node.value, int):
                return PrimType("int")
            elif isinstance(node.value, float):
                return PrimType("float")
            elif isinstance(node.value, str):
                return PrimType("str")
            elif isinstance(node.value, bool):
                return PrimType("bool")
            elif node.value is None:
                return PrimType("NoneType")
            return PrimType("Any")
            
        elif isinstance(node, ast.Name):
            if node.id in env:
                return self.deref(env[node.id])
            else:
                tv = self.fresh_var()
                env[node.id] = tv
                return tv
                
        elif isinstance(node, ast.BinOp):
            left_t = self.deref(self.analyze_expr(node.left, env))
            right_t = self.deref(self.analyze_expr(node.right, env))
            if isinstance(node.op, (ast.Add, ast.Sub, ast.Mult, ast.Div)):
                try:
                    self.unify(left_t, right_t)
                    return left_t
                except TypeError:
                    self.errors.append({
                        "line": node.lineno,
                        "column": node.col_offset,
                        "message": f"Binary operation type mismatch: {left_t} and {right_t}"
                    })
                    return PrimType("Any")
            return PrimType("Any")
            
        elif isinstance(node, ast.Call):
            func_t = self.deref(self.analyze_expr(node.func, env))
            arg_ts = [self.analyze_expr(arg, env) for arg in node.args]
            ret_t = self.fresh_var()
            expected_func = FuncType(arg_ts, ret_t)
            try:
                self.unify(func_t, expected_func)
            except TypeError:
                # If func_t is a primitive name like int() or str()
                if isinstance(node.func, ast.Name):
                    if node.func.id in ("int", "str", "bool", "float"):
                        return PrimType(node.func.id)
                self.errors.append({
                    "line": node.lineno,
                    "column": node.col_offset,
                    "message": f"Invalid function call signature for {node.func}"
                })
            return ret_t
            
        elif isinstance(node, ast.List):
            elem_t = self.fresh_var()
            for elt in node.elts:
                t = self.analyze_expr(elt, env)
                try:
                    self.unify(elem_t, t)
                except TypeError:
                    self.errors.append({
                        "line": node.lineno,
                        "column": node.col_offset,
                        "message": f"Heterogeneous list element type {t} inconsistent with {elem_t}"
                    })
            return ListType(elem_t)
            
        elif isinstance(node, ast.Dict):
            key_t = self.fresh_var()
            val_t = self.fresh_var()
            for k, v in zip(node.keys, node.values):
                if k is not None:
                    kt = self.analyze_expr(k, env)
                    self.unify(key_t, kt)
                vt = self.analyze_expr(v, env)
                self.unify(val_t, vt)
            return DictType(key_t, val_t)

        return PrimType("Any")

    def analyze_stmt(self, node: ast.AST, env: Dict[str, TypeNode]) -> None:
        if isinstance(node, ast.Assign):
            val_t = self.analyze_expr(node.value, env)
            for target in node.targets:
                if isinstance(target, ast.Name):
                    if target.id in env:
                        try:
                            self.unify(env[target.id], val_t)
                        except TypeError:
                            self.errors.append({
                                "line": node.lineno,
                                "column": node.col_offset,
                                "message": f"Reassignment type conflict for '{target.id}': expected {env[target.id]}, got {val_t}"
                            })
                    else:
                        env[target.id] = val_t
                    self.inferred_types[target.id] = repr(self.deref(val_t))
                    
        elif isinstance(node, ast.FunctionDef):
            arg_types = []
            for arg in node.args.args:
                tv = self.fresh_var()
                if arg.annotation:
                    if isinstance(arg.annotation, ast.Name):
                        tv = PrimType(arg.annotation.id)
                env[arg.arg] = tv
                arg_types.append(tv)
                
            ret_tv = self.fresh_var()
            if node.returns:
                if isinstance(node.returns, ast.Name):
                    ret_tv = PrimType(node.returns.id)
                    
            func_t = FuncType(arg_types, ret_tv)
            env[node.name] = func_t
            self.inferred_types[node.name] = repr(func_t)
            
            inner_env = dict(env)
            for stmt in node.body:
                self.analyze_stmt(stmt, inner_env)
                
        elif isinstance(node, ast.Return):
            if node.value:
                self.analyze_expr(node.value, env)
                
        elif isinstance(node, ast.Expr):
            self.analyze_expr(node.value, env)
            
        elif isinstance(node, ast.If):
            self.analyze_expr(node.test, env)
            for stmt in node.body:
                self.analyze_stmt(stmt, env)
            for stmt in node.orelse:
                self.analyze_stmt(stmt, env)

    def analyze(self, source_code: str, filename: str = "<string>") -> Dict[str, PyAny]:
        self.errors = []
        self.warnings = []
        self.inferred_types = {}
        self.type_var_counter = 0

        try:
            tree = ast.parse(source_code, filename=filename)
        except SyntaxError as e:
            self.errors.append({
                "line": e.lineno or 1,
                "column": e.offset or 1,
                "message": f"SyntaxError: {e.msg}"
            })
            return {
                "filename": filename,
                "errors": self.errors,
                "warnings": self.warnings,
                "inferred_types": self.inferred_types
            }

        # Build CFG
        builder = CFGBuilder()
        builder.visit(tree)

        # Global Environment
        env: Dict[str, TypeNode] = {}
        
        # Traverse AST statements
        for stmt in tree.body:
            self.analyze_stmt(stmt, env)

        # Dead code check: blocks without predecessors (excluding entry)
        for block in builder.cfg.blocks:
            if block != builder.cfg.entry_block and not block.predecessors:
                self.warnings.append({
                    "line": block.statements[0].lineno if block.statements else 1,
                    "column": block.statements[0].col_offset if block.statements else 1,
                    "message": f"Unreachable basic block detected (Block #{block.block_id})"
                })

        return {
            "filename": filename,
            "errors": self.errors,
            "warnings": self.warnings,
            "inferred_types": self.inferred_types
        }
