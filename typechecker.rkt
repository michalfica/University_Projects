#lang plait

; parser
; -----------

(define-type Expr
  (const-num [val : Number])
  (const-bool [val : Boolean])
  (var-expr [id : Symbol])
  (let-expr [id : Symbol] [e1 : Expr] [e2 : Expr])
  (if-expr [eb : Expr] [et : Expr] [ef : Expr])
  (fun [id : Symbol] [e : Expr])
  (app [fun : Expr] [arg : Expr]))

(define (make-apps e es)
  (if (empty? es)
      e
      (make-apps (app e (first es)) (rest es))))

(define (make-funs ids e)
  (if (empty? ids)
      e
      (fun (first ids) (make-funs (rest ids) e))))

(define (parse q)
  (cond
    [(s-exp-number? q) (const-num (s-exp->number q))]
    [(s-exp-symbol? q)
     (case (s-exp->symbol q)
       [(true) (const-bool #t)]
       [(false) (const-bool #f)]
       [else (var-expr (s-exp->symbol q))])]
    [(s-exp-list? q)
     (let ([ql (s-exp->list q)])
       (cond
         [(and (= (length ql) 3)
               (equal? (first ql) `let))
          (let ([ll (s-exp->list (second ql))])
            (let-expr (s-exp->symbol (first ll))
                      (parse (second ll))
                      (parse (third ql))))]
         [(and (= (length ql) 4)
               (equal? (first ql) `if))
          (if-expr (parse (second ql))
                   (parse (third ql))
                   (parse (fourth ql)))]
         [(and (= (length ql) 3)
               (equal? (first ql) `lambda))
          (make-funs (map s-exp->symbol (s-exp->list (second ql)))
                     (parse (third ql)))]
         [else
          (make-apps (parse (first ql))
                     (map parse (rest ql)))]))]))

; środowiska
; -----------

(define-type-alias (Env 'a) (Listof (Symbol * 'a)))

(define env-empty : (Env 'a)
  empty)

(define (env-lookup x env)
  (type-case (Env 'a) env
    [empty (error 'env-lookup (string-append "Unknown identifier"
                                             (to-string x)))]
    [(cons p xs)
     (if (eq? (fst p) x)
         (snd p)
         (env-lookup x (rest env)))]))

(define (env-add x v env) (cons (pair x v) env))

; ewaluacja
; -----------

(define-type Value
  (number-val [v : Number])
  (boolean-val [v : Boolean])
  (builtin [f : (Value -> Value)])
  (closure [id : Symbol] [body : Expr] [env : (Env Value)]))

(define (arith-op f)
  (lambda (x y)
    (number-val (f (number-val-v x) (number-val-v y)))))

(define (comp-op f)
  (lambda (x y)
    (boolean-val (f (number-val-v x) (number-val-v y)))))

(define (bool-op f)
  (lambda (x y)
    (boolean-val (f (boolean-val-v x) (boolean-val-v y)))))

(define (builtin-binary f)
  (builtin
   (lambda (x)
     (builtin
      (lambda (y)
        (f x y))))))

(define (name->builtin op)
  (case op
    ['+ (builtin-binary (arith-op +))]
    ['- (builtin-binary (arith-op -))]
    ['* (builtin-binary (arith-op *))]
    ['/ (builtin-binary (arith-op /))]
    ['= (builtin-binary (comp-op =))]
    ['< (builtin-binary (comp-op <))]
    ['and (builtin-binary (bool-op (lambda (x y) (and x y))))]
    ['or (builtin-binary (bool-op (lambda (x y) (or x y))))]))

(define builtins '(+ - * / = < and or))

(define start-env
  (foldr (lambda (x env) (env-add x (name->builtin x) env))
         env-empty builtins))

(define (eval-env e env)
  (type-case Expr e
    [(const-num n) (number-val n)]
    [(const-bool b) (boolean-val b)]
    [(var-expr x) (env-lookup x env)]
    [(let-expr x e1 e2)
     (eval-env e2 (env-add x (eval-env e1 env) env))]
    [(if-expr eb et ef)
     (if (boolean-val-v (eval-env eb env))
         (eval-env et env)
         (eval-env ef env))]
    [(fun x e) (closure x e env)]
    [(app e1 e2)
     (let ([v1 (eval-env e1 env)]
           [v2 (eval-env e2 env)])
       (type-case Value v1
         [(builtin f) (f v2)]
         [(closure x e cenv) (eval-env e (env-add x v2 cenv))]
         [else (error 'eval-env "Invalid applied value!")]))]))

(define (eval e) (eval-env e start-env))

; typechecker
; -----------

; typ typów

(define-type Type
  (number-type)
  (boolean-type)
  (function-type [t1 : Type] [t2 : Type])
  (var-type [id : Symbol]))

; generowanie świeżych zmiennych typowych

(define gen-symbol
  (local [(define count 0)]
    (lambda ()
      (begin
        (set! count (+ count 1))
        (string->symbol (string-append "a" (to-string count)))))))

(define (gen-var) (var-type (gen-symbol)))

; sprawdzenie czy a występuje w typie t

(define (occurs a t)
  (type-case Type t
    [(var-type b) (eq? a b)]
    [(number-type) #f]
    [(boolean-type) #f]
    [(function-type t1 t2) (or (occurs a t1) (occurs a t2))]))

; równania na typach i podstawienia

(define-type-alias TypeEq (Type * Type))
(define-type-alias TypeEqs (Listof TypeEq))
(define-type-alias Subst (Listof (Symbol * Type)))

; definicje dla równań na typach

(define (typeeq-l [eq : TypeEq]) (fst eq))
(define (typeeq-r [eq : TypeEq]) (snd eq))

(define typeeqs-empty : TypeEqs
  empty)

(define (typeeqs-single t1 t2) : TypeEqs
  (list (pair t1 t2)))

(define (typeeqs-append eqs1 eqs2) : TypeEqs
  (append eqs1 eqs2))

(define (typeeqs-concat eqss) : TypeEqs
  (foldr typeeqs-append typeeqs-empty eqss))

; definicje dla podstawień

(define subst-empty : Subst
  empty)

(define (subst-single a t) : Subst
  (list (pair a t)))

(define (subst-lookup s a)
  (type-case Subst s
    [empty (none)]
    [(cons p ps)
     (if (eq? (fst p) a)
         (some (snd p))
         (subst-lookup (rest s) a))]))

(define (substitute-var s a)
  (type-case (Optionof Type) (subst-lookup s a)
    [(none) (var-type a)]
    [(some t) t]))

(define (substitute s t)
  (type-case Type t
    [(var-type a) (substitute-var s a)]
    [(number-type) (number-type)]
    [(boolean-type) (boolean-type)]
    [(function-type t1 t2)
     (function-type (substitute s t1)
                    (substitute s t2))]))

(define (substitute-typeeqs s eqs) : TypeEqs
  (map (lambda (p) (pair (substitute s (typeeq-l p))
                         (substitute s (typeeq-r p)))) eqs))

(define (substitute-subst s s2) : Subst
  (map (lambda (p) (pair (fst p) (substitute s (snd p)))) s2))

(define (subst-compose s1 s2) : Subst
  (append s1 (substitute-subst s1 s2)))

; sprawdzanie typów: generowanie równań na typach

(define-type TypecheckResult
  (typecheck-result [tp : Type] [eqs : TypeEqs]))

(define (function2-type t1 t2 t3)
  (function-type t1 (function-type t2 t3)))

(define (name->type op)
  (case op
    [(+ - * /) (function2-type (number-type) (number-type) (number-type))]
    [(= <) (function2-type (number-type) (number-type) (boolean-type))]
    [(and or) (function2-type (boolean-type) (boolean-type) (boolean-type))]))

(define start-type-env
  (foldr (lambda (x env) (env-add x (name->type x) env)) env-empty builtins))

(define (typecheck-env e env)
  (type-case Expr e
    [(const-num n)
     (typecheck-result (number-type) typeeqs-empty)]
    [(const-bool b)
     (typecheck-result (boolean-type) typeeqs-empty)]
    [(var-expr x)
     (typecheck-result (env-lookup x env) typeeqs-empty)]
    [(app e1 e2)
     (let* ([r1 (typecheck-env e1 env)]
            [r2 (typecheck-env e2 env)]
            [arg-t (typecheck-result-tp r2)]
            [res-t (gen-var)])
       (typecheck-result res-t
                         (typeeqs-concat
                          (list (typeeqs-single (typecheck-result-tp r1)
                                                (function-type arg-t res-t))
                                (typecheck-result-eqs r1)
                                (typecheck-result-eqs r2)))))]
    [(fun x e)
     (let* ([arg-t (gen-var)]
            [r (typecheck-env e (env-add x arg-t env))]
            [res-t (typecheck-result-tp r)])
       (typecheck-result (function-type arg-t res-t)
                         (typecheck-result-eqs r)))]
    [(let-expr x e1 e2)
     (let* ([r1 (typecheck-env e1 env)]
            [r2 (typecheck-env e2 (env-add x (typecheck-result-tp r1) env))])
       (typecheck-result (typecheck-result-tp r2)
                         (typeeqs-append (typecheck-result-eqs r1)
                                         (typecheck-result-eqs r2))))]
    [(if-expr eb et ef)
     (let* ([rb (typecheck-env eb env)]
            [rt (typecheck-env et env)]
            [rf (typecheck-env ef env)])
       (typecheck-result (typecheck-result-tp rt)
                         (typeeqs-concat
                          (list (typeeqs-single (typecheck-result-tp rb)
                                                (boolean-type))
                                (typeeqs-single (typecheck-result-tp rt)
                                                (typecheck-result-tp rf))
                                (typecheck-result-eqs rb)
                                (typecheck-result-eqs rt)
                                (typecheck-result-eqs rf)))))]))

; algorytm unifikacji: rozwiązywanie równań na typach

(define (unify eqs)
  (local
    [(define (iter s eqs)
       (type-case TypeEqs eqs
         [empty (some s)]
         [(cons eq eqs1)
          (let ([eql (typeeq-l eq)]
                [eqr (typeeq-r eq)])
            (cond
              ; usunięcie
              [(and (number-type? eql)
                    (number-type? eqr))
               (iter s eqs1)]
              ; usunięcie
              [(and (boolean-type? eql)
                    (boolean-type? eqr))
               (iter s eqs1)]
              ; dekompozycja
              [(and (function-type? eql)
                    (function-type? eqr))
               (iter s (typeeqs-concat
                        (list (typeeqs-single (function-type-t1 eql)
                                              (function-type-t1 eqr))
                              (typeeqs-single (function-type-t2 eql)
                                              (function-type-t2 eqr))
                              eqs1)))]
              ; eliminacja
              [(var-type? eql)
               (if (occurs (var-type-id eql) eqr)
                   (none)
                   (type-case (Optionof Type) (subst-lookup s (var-type-id eql))
                     [; jeśli zmiennej nie ma w rozwiązaniu, rozszerzamy je
                      (none)
                      (let ([s1 (subst-single (var-type-id eql) eqr)])
                        (iter (subst-compose s1 s)
                              (substitute-typeeqs s1 eqs1)))]
                     [; jeśli zmienna jest w rozwiązaniu, dodajemy równanie
                      (some t)
                      (iter s (typeeqs-append (typeeqs-single t eqr) eqs1))]))]
              ; zamiana
              [(var-type? eqr)
               (iter s (typeeqs-append (typeeqs-single eqr eql) eqs1))]
              ; konflikt
              [else (none)]))]))]
    (iter subst-empty eqs)))

; sprawdzanie typu wyrażenia:
; generuje równania na typach i rozwiązuje je

(define (typecheck e)
  (let [(r (typecheck-env e start-type-env))]
    (type-case (Optionof Subst) (unify (typecheck-result-eqs r))
      [(none) (error 'typecheck "Type error")]
      [(some s) (substitute s (typecheck-result-tp r))])))

(define (typecheck-and-eval e)
  (pair (typecheck e) (eval e)))

; przykład z wykładu
(typecheck-and-eval (parse `((lambda (x) (+ x 1)) 2)))
; przykład: błąd typu
(typecheck-and-eval (parse `((lambda (x) (+ x true)) 2)))