// 测试 toggleComment（VSCode trackSelection 方式）
#include <QApplication>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextBlock>
#include <QVector>
#include <QDebug>

struct R { QString text; int anchor; int active; };

R doToggle(const QString &inputText, int anchorPos, int activePos)
{
    QTextDocument doc(inputText);

    int anchorBlock = doc.findBlock(anchorPos).blockNumber();
    int activeBlock = doc.findBlock(activePos).blockNumber();
    int startBlock = qMin(anchorBlock, activeBlock);
    int endBlock = qMax(anchorBlock, activeBlock);

    bool allCommented = true;
    for (int i = startBlock; i <= endBlock; i++) {
        QString t = doc.findBlockByNumber(i).text().trimmed();
        if (!t.isEmpty() && !t.startsWith("--")) { allCommented = false; break; }
    }

    // 保存原始行位置
    QVector<int> origStarts(endBlock - startBlock + 1);
    for (int i = startBlock; i <= endBlock; i++)
        origStarts[i - startBlock] = doc.findBlockByNumber(i).position();

    // 从后往前修改
    QVector<int> deltas(endBlock - startBlock + 1);
    for (int i = endBlock; i >= startBlock; i--) {
        QTextBlock block = doc.findBlockByNumber(i);
        QTextCursor lc(&doc);
        lc.setPosition(block.position());
        lc.setPosition(block.position() + block.length() - 1, QTextCursor::KeepAnchor);
        QString line = lc.selectedText();

        if (allCommented) {
            int idx = line.indexOf("--");
            if (idx >= 0) {
                int len = (line.length() > idx + 2 && line.at(idx + 2) == ' ') ? 3 : 2;
                line.remove(idx, len);
                deltas[i - startBlock] = -len;
            }
        } else {
            line.prepend("-- ");
            deltas[i - startBlock] = 3;
        }
        lc.insertText(line);
    }

    // 恢复选区：both shift by cumulative delta where modification position <= original position
    int rd = 0, na = anchorPos, nv = activePos;
    for (int i = startBlock; i <= endBlock; i++) {
        rd += deltas[i - startBlock];
        int origStart = origStarts[i - startBlock];
        if (anchorPos >= origStart) na = anchorPos + rd;
        if (activePos >= origStart) nv = activePos + rd;
    }
    na = qMax(0, na); nv = qMax(0, nv);

    return {doc.toPlainText(), na, nv};
}

bool tc(const char *name, const QString &text, int a, int v,
        const QString &et, int ea, int ev)
{
    auto r = doToggle(text, a, v);
    bool ok = (r.text == et && r.anchor == ea && r.active == ev);
    qDebug() << (ok ? "✅" : "❌") << name;
    if (!ok) {
        qDebug() << "  got:    text=[" << r.text << "] a=" << r.anchor << " v=" << r.active;
        qDebug() << "  expect: text=[" << et << "] a=" << ea << " v=" << ev;
    }
    return ok;
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QString code = "line0\nline1\nline2\nline3";
    QString commented = "-- line0\n-- line1\n-- line2\nline3";
    int p=0, f=0;

    qDebug() << "=== 添加注释 ===";

    // 光标@行首 → anchor和active都移到"-- "后
    (tc("光标L1C0", code, 6, 6,
        "line0\n-- line1\nline2\nline3", 9, 9)) ? p++ : f++;

    // 单行选区 → anchor和active都跟着文本走
    (tc("单行L1C0-L1C5", code, 6, 11,
        "line0\n-- line1\nline2\nline3", 9, 14)) ? p++ : f++;

    // 单行行内
    (tc("单行L1C2-L1C5", code, 8, 11,
        "line0\n-- line1\nline2\nline3", 11, 14)) ? p++ : f++;

    // 多行 → 都走
    (tc("多行L0C0-L2C5", code, 0, 17,
        "-- line0\n-- line1\n-- line2\nline3", 3, 26)) ? p++ : f++;

    (tc("多行L0C0-L2C0", code, 0, 12,
        "-- line0\n-- line1\n-- line2\nline3", 3, 21)) ? p++ : f++;

    (tc("多行L1C3-L2C3", code, 9, 15,
        "line0\n-- line1\n-- line2\nline3", 12, 21)) ? p++ : f++;

    (tc("多行L0C2-L2C3", code, 2, 15,
        "-- line0\n-- line1\n-- line2\nline3", 5, 24)) ? p++ : f++;

    qDebug() << "\n=== 取消注释 ===";

    (tc("取消 光标L1行首", commented, 9, 9,
        "-- line0\nline1\n-- line2\nline3", 6, 6)) ? p++ : f++;

    (tc("取消 单行整行", commented, 9, 17,
        "-- line0\nline1\n-- line2\nline3", 6, 14)) ? p++ : f++;

    (tc("取消 多行L0C0-L2C0", commented, 0, 21,
        "line0\nline1\nline2\nline3", 0, 12)) ? p++ : f++;

    (tc("取消 多行L0C0-L2C8", commented, 0, 26,
        "line0\nline1\nline2\nline3", 0, 17)) ? p++ : f++;

    (tc("取消 多行L1C3-L2C3", commented, 12, 21,
        "-- line0\nline1\nline2\nline3", 9, 15)) ? p++ : f++;

    qDebug() << "\n=== 边界 ===";

    (tc("光标L0C0", code, 0, 0,
        "-- line0\nline1\nline2\nline3", 3, 3)) ? p++ : f++;

    (tc("光标L3C0", code, 18, 18,
        "line0\nline1\nline2\n-- line3", 21, 21)) ? p++ : f++;

    (tc("反复:注释后取消", "-- line0\n-- line1\n-- line2\nline3", 0, 26,
        "line0\nline1\nline2\nline3", 0, 17)) ? p++ : f++;

    (tc("反复:取消后注释", "line0\nline1\nline2\nline3", 0, 17,
        "-- line0\n-- line1\n-- line2\nline3", 3, 26)) ? p++ : f++;

    (tc("多行anchor@L1C3 active@L2C0", code, 9, 12,
        "line0\n-- line1\n-- line2\nline3", 12, 18)) ? p++ : f++;

    qDebug() << "\n结果: 通过" << p << "失败" << f;
    return f > 0 ? 1 : 0;
}
