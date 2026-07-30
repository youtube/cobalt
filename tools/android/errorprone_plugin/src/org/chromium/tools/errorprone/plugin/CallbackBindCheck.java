// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.tools.errorprone.plugin;

import static com.google.errorprone.matchers.Matchers.instanceMethod;

import com.google.errorprone.BugPattern;
import com.google.errorprone.VisitorState;
import com.google.errorprone.bugpatterns.BugChecker;
import com.google.errorprone.bugpatterns.BugChecker.LambdaExpressionTreeMatcher;
import com.google.errorprone.fixes.SuggestedFix;
import com.google.errorprone.matchers.Description;
import com.google.errorprone.matchers.Matcher;
import com.google.errorprone.util.ASTHelpers;
import com.sun.source.tree.ArrayAccessTree;
import com.sun.source.tree.BlockTree;
import com.sun.source.tree.ExpressionStatementTree;
import com.sun.source.tree.ExpressionTree;
import com.sun.source.tree.IdentifierTree;
import com.sun.source.tree.LambdaExpressionTree;
import com.sun.source.tree.LiteralTree;
import com.sun.source.tree.MemberSelectTree;
import com.sun.source.tree.MethodInvocationTree;
import com.sun.source.tree.NewClassTree;
import com.sun.source.tree.StatementTree;
import com.sun.source.tree.Tree;
import com.sun.source.util.TreeScanner;
import com.sun.tools.javac.code.Symbol;
import com.sun.tools.javac.code.Type;

import org.chromium.build.annotations.ServiceImpl;

import javax.lang.model.element.ElementKind;

/** Checks for `() -> callback.onResult(foo)` and suggests `callback.bind(foo)`. */
@ServiceImpl(BugChecker.class)
@BugPattern(
        name = "CallbackBind",
        summary = "Use callback.bind(foo) instead of () -> callback.onResult(foo)",
        severity = BugPattern.SeverityLevel.WARNING,
        linkType = BugPattern.LinkType.CUSTOM,
        link = "https://chromium.googlesource.com/chromium/src/+/main/styleguide/java/java.md")
public class CallbackBindCheck extends BugChecker implements LambdaExpressionTreeMatcher {
    private static final Matcher<ExpressionTree> ON_RESULT_MATCHER =
            instanceMethod().onDescendantOf("org.chromium.base.Callback").named("onResult");

    @Override
    public Description matchLambdaExpression(LambdaExpressionTree tree, VisitorState state) {
        // Condition 1: Lambda has no parameters.
        if (!tree.getParameters().isEmpty()) {
            return Description.NO_MATCH;
        }

        // Condition 2: Target type is Runnable.
        Type type = ASTHelpers.getType(tree);
        if (type == null) {
            return Description.NO_MATCH;
        }
        if (!ASTHelpers.isSameType(type, state.getTypeFromString("java.lang.Runnable"), state)) {
            return Description.NO_MATCH;
        }

        // Condition 3: Body is single instruction: callback.onResult(foo)
        ExpressionTree expression = getSingleExpression(tree.getBody());
        if (expression == null) {
            return Description.NO_MATCH;
        }

        if (!ON_RESULT_MATCHER.matches(expression, state)) {
            return Description.NO_MATCH;
        }

        MethodInvocationTree invocation = (MethodInvocationTree) expression;
        if (invocation.getArguments().size() != 1) {
            return Description.NO_MATCH;
        }

        ExpressionTree callback = ASTHelpers.getReceiver(invocation);
        if (callback == null) {
            return Description.NO_MATCH;
        }
        ExpressionTree argument = invocation.getArguments().get(0);

        // Condition 4: Receiver and Argument must be purely local (no fields, no method calls).
        if (!isPurelyLocal(callback, state) || !isPurelyLocal(argument, state)) {
            return Description.NO_MATCH;
        }

        String callbackSource = state.getSourceForNode(callback);
        String argumentSource = state.getSourceForNode(argument);

        SuggestedFix fix =
                SuggestedFix.replace(tree, callbackSource + ".bind(" + argumentSource + ")");

        return describeMatch(tree, fix);
    }

    private ExpressionTree getSingleExpression(Tree body) {
        if (body instanceof ExpressionTree) {
            return (ExpressionTree) body;
        } else if (body instanceof BlockTree) {
            BlockTree block = (BlockTree) body;
            if (block.getStatements().size() == 1) {
                StatementTree statement = block.getStatements().get(0);
                if (statement instanceof ExpressionStatementTree) {
                    return ((ExpressionStatementTree) statement).getExpression();
                }
            }
        }
        return null;
    }

    private boolean isPurelyLocal(ExpressionTree expression, VisitorState state) {
        Boolean result =
                expression.accept(
                        new TreeScanner<Boolean, Void>() {
                            @Override
                            public Boolean visitArrayAccess(ArrayAccessTree node, Void p) {
                                return false;
                            }

                            @Override
                            public Boolean visitIdentifier(IdentifierTree node, Void p) {
                                if (node.getName().contentEquals("this")
                                        || node.getName().contentEquals("super")) {
                                    return true;
                                }
                                Symbol symbol = ASTHelpers.getSymbol(node);
                                if (symbol == null) {
                                    return false;
                                }
                                if (symbol.getKind() == ElementKind.LOCAL_VARIABLE
                                        || symbol.getKind() == ElementKind.PARAMETER) {
                                    return true;
                                }
                                return false;
                            }

                            @Override
                            public Boolean visitMemberSelect(MemberSelectTree node, Void p) {
                                Symbol symbol = ASTHelpers.getSymbol(node);
                                if (symbol != null && symbol.getKind() == ElementKind.FIELD) {
                                    return false;
                                }
                                return super.visitMemberSelect(node, p);
                            }

                            @Override
                            public Boolean visitMethodInvocation(
                                    MethodInvocationTree node, Void p) {
                                return false;
                            }

                            @Override
                            public Boolean visitNewClass(NewClassTree node, Void p) {
                                return false;
                            }

                            @Override
                            public Boolean visitLiteral(LiteralTree node, Void p) {
                                return true;
                            }

                            @Override
                            public Boolean reduce(Boolean r1, Boolean r2) {
                                if (r1 == null) return r2;
                                if (r2 == null) return r1;
                                return r1 && r2;
                            }
                        },
                        null);
        return result != null && result;
    }
}
