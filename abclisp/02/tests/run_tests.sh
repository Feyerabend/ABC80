#!/bin/bash
# abclisp test suite
#
# Each REPL line must be a *complete* s-expression — the REPL calls
# parse_expr() once per fgets() line, so multi-line forms are not
# supported in batch mode.
#
# Usage:  bash tests/run_tests.sh        (from project root)
#         LISP=./lisp bash tests/run_tests.sh

LISP="${LISP:-./lisp}"
TEST_VM="${TEST_VM:-./test_vm}"
PASS=0
FAIL=0

# check <description> <newline-separated REPL input> <expected substring>
check() {
    local desc="$1"
    local input="$2"
    local expected="$3"
    local actual
    actual=$(printf '%s\n' "$input" | "$LISP" 2>/dev/null)
    if [[ "$actual" == *"$expected"* ]]; then
        printf "PASS  %s\n" "$desc"
        ((PASS++))
    else
        printf "FAIL  %s\n" "$desc"
        printf "      input:    %s\n" "$(echo "$input" | head -3 | tr '\n' '|')"
        printf "      expected: %s\n" "$expected"
        printf "      got:      %s\n" "$actual"
        ((FAIL++))
    fi
}

# ------------------------------------------------------------------
echo "--- Arithmetic ---"
check "add"          "(+ 3 4)"                               "7"
check "subtract"     "(- 10 3)"                              "7"
check "multiply"     "(* 6 7)"                               "42"
check "negative"     "(- 0 5)"                               "-5"
check "nested"       "(+ (* 2 3) (* 4 5))"                  "26"
check "12-bit wrap"  "(+ 2047 1)"                            "-2048"

# ------------------------------------------------------------------
echo "--- Comparison & boolean ---"
check "eq true"      "(= 7 7)"                               "#t"
check "eq false"     "(= 7 8)"                               "#f"
check "lt true"      "(< 3 5)"                               "#t"
check "lt false"     "(< 5 3)"                               "#f"
check "not #f"       "(not #f)"                              "#t"
check "not #t"       "(not #t)"                              "#f"
check "not nil"      "(not nil)"                             "#t"
check "and both"     "(and #t #t)"                           "#t"
check "and short-ckt" "$(printf '(and #f (error 1))')"       "#f"
check "or short-ckt" "$(printf '(or #t (error 1))')"         "#t"

# ------------------------------------------------------------------
echo "--- Variables ---"
check "define val"   "$(printf '(define x 99)\n(+ x 1)')"   "100"
check "set!"         "$(printf '(define y 1)\n(set! y 42)\ny')" "42"
check "shorthand def" "$(printf '(define (double n) (* n 2))\n(double 21)')" "42"

# ------------------------------------------------------------------
echo "--- If / cond ---"
check "if true"      "(if #t 1 2)"                          "1"
check "if false"     "(if #f 1 2)"                          "2"
check "if no-alt"    "(if #f 99)"                           ""
check "cond first"   "(cond ((= 1 1) 10) (else 20))"       "10"
check "cond else"    "(cond ((= 1 2) 10) (else 20))"       "20"
check "cond multi"   "(cond ((= 1 2) 10) ((= 2 2) 20) (else 30))" "20"

# ------------------------------------------------------------------
echo "--- Lambda & closures ---"
check "lambda call"  "((lambda (x) (* x x)) 7)"             "49"
check "two args"     "((lambda (a b) (+ a b)) 3 4)"         "7"
check "closure make" "$(printf '(define (make-adder n) (lambda (x) (+ x n)))\n(define add5 (make-adder 5))\n(add5 10)')" "15"
check "two closures" "$(printf '(define (make-adder n) (lambda (x) (+ x n)))\n(define add5 (make-adder 5))\n(define add10 (make-adder 10))\n(+ (add5 1) (add10 1))')" "17"

# ------------------------------------------------------------------
echo "--- Let & letrec ---"
check "let basic"    "(let ((x 3) (y 4)) (+ x y))"         "7"
check "let body"     "(let ((x 2)) (+ x x))"               "4"
check "letrec fact"  "(letrec ((fact (lambda (n) (if (= n 0) 1 (* n (fact (- n 1))))))) (fact 6))" "720"
check "letrec even?" "$(printf '(letrec ((even? (lambda (n) (if (= n 0) #t (odd? (- n 1))))) (odd? (lambda (n) (if (= n 0) #f (even? (- n 1)))))) (even? 10))')" "#t"
check "letrec odd?"  "$(printf '(letrec ((even? (lambda (n) (if (= n 0) #t (odd? (- n 1))))) (odd? (lambda (n) (if (= n 0) #f (even? (- n 1)))))) (odd? 7))')"  "#t"

# ------------------------------------------------------------------
echo "--- Tail-call optimisation ---"
# count 500 would exhaust the call stack without TCO (MAX_CALLS=16)
check "TCO count"    "$(printf '(define (count n) (if (= n 0) (quote done) (count (- n 1))))\n(count 500)')" "done"
# sum 1..100 = 5050 — wraps in 12-bit signed to 954
check "TCO accum"    "$(printf '(define (sum n acc) (if (= n 0) acc (sum (- n 1) (+ acc n))))\n(sum 100 0)')" "954"

# ------------------------------------------------------------------
echo "--- Lists ---"
check "cons car"     "(car (cons 1 2))"                     "1"
check "cons cdr"     "(cdr (cons 1 2))"                     "2"
check "list 3"       "(list 1 2 3)"                         "(1 2 3)"
check "car list"     "(car (list 10 20 30))"                "10"
check "cdr list"     "(cdr (list 1 2 3))"                   "(2 3)"
check "pair? true"   "(pair? (list 1))"                     "#t"
check "pair? false"  "(pair? 42)"                           "#f"
check "null? nil"    "(null? nil)"                          "#t"
check "null? list"   "(null? (list 1))"                     "#f"
check "append"       "(append (list 1 2) (list 3 4))"       "(1 2 3 4)"
check "nested list"  "(list (list 1 2) (list 3 4))"         "((1 2) (3 4))"
check "quote list"   "(quote (a b c))"                      "(a b c)"

# ------------------------------------------------------------------
echo "--- Characters ---"
check "char? true"   "(char? #\\A)"                         "#t"
check "char? false"  "(char? 42)"                           "#f"
check "char->int A"  "(char->int #\\A)"                     "65"
check "char->int nl" "(char->int #\\newline)"               "10"
check "char->int sp" "(char->int #\\space)"                 "32"
check "int->char"    "(display (int->char 65))"             "A"

# ------------------------------------------------------------------
echo "--- Strings ---"
check "str-len"      "(str-len \"hello\")"                  "5"
check "str-ref"      "(display (str-ref \"hello\" 1))"      "e"
check "str-append"   "(display (str-append \"foo\" \"bar\"))" "foobar"
check "str=? true"   "(str=? \"abc\" \"abc\")"              "#t"
check "str=? false"  "(str=? \"abc\" \"xyz\")"              "#f"
check "num->str"     "(display (num->str 42))"              "42"
check "str->num"     "(str->num \"123\")"                   "123"
check "sym->str"     "(display (sym->str (quote hello)))"   "hello"
check "str->sym"     "(str->sym \"world\")"                 "world"
check "string?"      "(string? \"hi\")"                     "#t"
check "string? no"   "(string? 42)"                         "#f"

# ------------------------------------------------------------------
echo "--- Quasiquote ---"
check "quasi val"    "$(printf '(define x 42)\n`(the answer is ,x)')" "(the answer is 42)"
check "quasi arith"  "$(printf '(define a 3)\n(define b 4)\n`(,a + ,b = ,(+ a b))')" "(3 + 4 = 7)"
check "quasi fn"     "$(printf '(define (mk-pt x y) `(point ,x ,y))\n(mk-pt 3 4)')" "(point 3 4)"
check "list builtin" "$(printf '(define (range a b) (list a b))\n(range 10 20)')" "(10 20)"

# ------------------------------------------------------------------
echo "--- Write / display ---"
check "write string"  "(write \"hello\")"                    "\"hello\""
check "write char"    "(write #\\Z)"                         "#\\Z"
check "write #\\space""(write #\\space)"                     "#\\space"
check "write list"    "(write (list 1 \"two\" #t))"          "(1 \"two\" #t)"
check "write #t"      "(write #t)"                           "#t"
check "write #f"      "(write #f)"                           "#f"
check "write int"     "(write 42)"                           "42"
check "display str"   "(display \"no quotes\")"              "no quotes"
check "display char"  "(display #\\A)"                       "A"

# ------------------------------------------------------------------
echo "--- Apply ---"
check "apply 2-arg"  "$(printf '(define (add a b) (+ a b))\n(apply add (list 3 4))')" "7"
check "apply 1-arg"  "$(printf '(define (neg x) (- 0 x))\n(apply neg (list 5))')" "-5"

# ------------------------------------------------------------------
echo "--- Begin ---"
check "begin seq"    "$(printf '(begin (define x 1) (define x 2) x)')" "2"

# ------------------------------------------------------------------
echo "--- Error ---"
check "error 42"     "(error 42)"                            "E:42"
check "error 0"      "(error 0)"                             "E:0"

# ------------------------------------------------------------------
echo "--- Z80 transpilation harness ---"
if [[ -x "$TEST_VM" ]]; then
    vm_out=$("$TEST_VM" 2>/dev/null)
    vm_pass=$(echo "$vm_out" | grep -c '^PASS')
    vm_fail=$(echo "$vm_out" | grep -c '^FAIL')
    PASS=$((PASS + vm_pass))
    FAIL=$((FAIL + vm_fail))
    echo "$vm_out" | grep -E '^(PASS|FAIL|---)'
else
    echo "  (skipped — build test_vm first: cc -std=c11 -O2 -Wno-unused-function -o test_vm test_vm.c)"
fi

# ------------------------------------------------------------------
echo ""
echo "Results: $PASS passed, $FAIL failed  (total $((PASS+FAIL)))"
[[ $FAIL -eq 0 ]] && echo "ALL PASS" && exit 0 || { echo "SOME FAILURES"; exit 1; }
