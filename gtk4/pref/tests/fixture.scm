;;;
;;;   Copyright (c) 2026 uim Project https://github.com/uim/uim
;;;
;;;   All rights reserved.
;;;
;;;   Redistribution and use in source and binary forms, with or without
;;;   modification, are permitted provided that the following conditions
;;;   are met:
;;;
;;;   1. Redistributions of source code must retain the above copyright
;;;      notice, this list of conditions and the following disclaimer.
;;;   2. Redistributions in binary form must reproduce the above copyright
;;;      notice, this list of conditions and the following disclaimer in the
;;;      documentation and/or other materials provided with the distribution.
;;;   3. Neither the name of authors nor the names of its contributors
;;;      may be used to endorse or promote products derived from this software
;;;      without specific prior written permission.
;;;
;;;   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
;;;   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
;;;   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
;;;   ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
;;;   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
;;;   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
;;;   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
;;;   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
;;;   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
;;;   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
;;;   POSSIBILITY OF SUCH DAMAGE.
;;;

;; Custom variables used by the uim-pref-gtk4 tests. Every custom type
;; that gtk4/pref can edit appears here once, in its own primary group
;; so that the tests do not depend on the customs uim itself ships.

(define-custom-group 'pref-test
  "Pref test"
  "Custom variables for the uim-pref-gtk4 tests")

(define-custom-group 'pref-test-sub
  "Pref test sub"
  "A subgroup that gets a frame of its own")

(define-custom 'pref-test-bool #t
  '(pref-test)
  '(boolean)
  "Test boolean"
  "A boolean")

(define-custom 'pref-test-int 10
  '(pref-test)
  '(integer 0 100)
  "Test integer"
  "An integer that is active only while pref-test-bool is true")

(custom-add-hook 'pref-test-int
  'custom-activity-hooks
  (lambda ()
    pref-test-bool))

(define-custom 'pref-test-str "hello"
  '(pref-test)
  '(string ".*")
  "Test string"
  "A string")

(define-custom 'pref-test-file "/tmp/pref-test-file"
  '(pref-test pref-test-sub)
  '(pathname regular-file)
  "Test file"
  "A regular file")

(define-custom 'pref-test-dir "/tmp"
  '(pref-test pref-test-sub)
  '(pathname directory)
  "Test directory"
  "A directory")

(define-custom 'pref-test-choice 'beta
  '(pref-test pref-test-sub)
  (list 'choice
        (list 'alpha "Alpha" "The first")
        (list 'beta "Beta" "The second")
        (list 'gamma "Gamma" "The third"))
  "Test choice"
  "A choice")

(define-custom 'pref-test-olist '(one two)
  '(pref-test pref-test-sub)
  (list 'ordered-list
        (list 'one "One" "The first")
        (list 'two "Two" "The second")
        (list 'three "Three" "The third"))
  "Test ordered list"
  "An ordered list")

(define-custom 'pref-test-key '("<Control>j" "escape")
  '(pref-test pref-test-sub)
  '(key)
  "Test key"
  "A key")

(define-custom 'pref-test-table '(("a" "x") ("b" "y"))
  '(pref-test pref-test-sub)
  (list 'table
        (list 'input "Input" "Input")
        (list 'output "Output" "Output"))
  "Test table"
  "A table")
