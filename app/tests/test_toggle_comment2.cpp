// 调试 toggleComment - 查看每个步骤
#include <QApplication>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextBlock>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QString code = "line0\nline1\nline2\nline3";
    QTextDocument doc(code);

    qDebug() << "=== 文档结构 ===";
    qDebug() << "Full text:" << doc.toPlainText();
    qDebug() << "Block count:" << doc.blockCount();
    for (int i = 0; i < doc.blockCount(); i++) {
        QTextBlock b = doc.findBlockByNumber(i);
        qDebug() << "  Block" << i << ": pos=" << b.position()
                 << "len=" << b.length()
                 << "text=[" << b.text() << "]";
    }

    // 测试: 选中 L0C0-L2C5, 添加注释
    qDebug() << "\n=== 测试: 选中 L0C0(pos0) - L2C5(pos17), 添加注释 ===";

    int anchorPos = 0;   // L0C0
    int activePos = 17;  // L2C5

    int anchorBlock = doc.findBlock(anchorPos).blockNumber();  // 0
    int anchorCol = anchorPos - doc.findBlock(anchorPos).position();  // 0
    int activeBlock = doc.findBlock(activePos).blockNumber();  // 2
    int activeCol = activePos - doc.findBlock(activePos).position();  // 5

    qDebug() << "anchor: block=" << anchorBlock << "col=" << anchorCol;
    qDebug() << "active: block=" << activeBlock << "col=" << activeCol;

    int startBlock = qMin(anchorBlock, activeBlock);  // 0
    int endBlock = qMax(anchorBlock, activeBlock);    // 2
    qDebug() << "range:" << startBlock << "-" << endBlock;

    // 判断注释状态
    bool allCommented = true;
    for (int i = startBlock; i <= endBlock; i++) {
        QString text = doc.findBlockByNumber(i).text();
        QString trimmed = text.trimmed();
        qDebug() << "  block" << i << ": trimmed=[" << trimmed << "] startsWith-- =" << trimmed.startsWith("--");
        if (!trimmed.isEmpty() && !trimmed.startsWith("--")) {
            allCommented = false;
        }
    }
    qDebug() << "allCommented=" << allCommented;

    int delta = allCommented ? -3 : 3;
    qDebug() << "delta=" << delta;

    // 逐行修改
    qDebug() << "\n--- 修改 ---";
    for (int i = endBlock; i >= startBlock; i--) {
        QTextBlock block = doc.findBlockByNumber(i);
        qDebug() << "  处理 block" << i << ": pos=" << block.position() << "len=" << block.length();

        QTextCursor lc(&doc);
        lc.setPosition(block.position());
        lc.setPosition(block.position() + block.length() - 1, QTextCursor::KeepAnchor);
        QString line = lc.selectedText();
        qDebug() << "    selectedText=[" << line << "]";

        line.prepend("-- ");
        qDebug() << "    newLine=[" << line << "]";

        lc.insertText(line);
        qDebug() << "    文档现在: [" << doc.toPlainText() << "]";

        // 验证 block 结构
        for (int j = 0; j < doc.blockCount(); j++) {
            QTextBlock b = doc.findBlockByNumber(j);
            qDebug() << "      block" << j << ": pos=" << b.position() << "text=[" << b.text() << "]";
        }
    }

    // 恢复选区
    int blockStart0 = doc.findBlockByNumber(startBlock).position();

    int newAnchor = anchorPos;
    if (anchorBlock > startBlock || (anchorBlock == startBlock && anchorCol > 0))
        newAnchor += delta;
    else if (anchorBlock == startBlock && anchorCol == 0)
        newAnchor = blockStart0;

    int newActive = activePos + delta;
    newAnchor = qMax(0, newAnchor);
    newActive = qMax(0, newActive);

    qDebug() << "\n结果:";
    qDebug() << "  text: [" << doc.toPlainText() << "]";
    qDebug() << "  newAnchor=" << newAnchor << " newActive=" << newActive;

    // 期望: "-- line0\n-- line1\n-- line2\nline3"
    // L0: "-- line0" (8 chars + \n)
    // L1: "-- line1" (8 chars + \n)
    // L2: "-- line2" (8 chars + \n)
    // L3: "line3"
    // active 应该在 "-- line2" 的 '2' 后面 = 8+1+8+1+8+5 = 31... 不对
    // 原始 active=17 是 L2C5, 即 "line2" 的 '2' 位置
    // 注释后 L2 变成 "-- line2", '2' 在第 8 个字符, 但 "line2" 只有 5 个字符...
    // 不对, L2C5 = 第 2 行的第 5 列 = '2' 的后面 = pos 12+5=17
    // 注释后: "-- line2" 的长度 = 8, '\n' 不算在 block.length 里
    // L2C5 应该变成 "-- line2" 的第 8 个字符后面 = blockStart(21) + 8 = 29
    // 但 delta=3*3=9, 所以 17+9=26... 不对

    // 啊! delta 应该是每行分别计算的!
    // 当前 delta 是固定的 3, 但对于 active 在第 2 行的情况,
    // delta 应该是 3 (L0) + 3 (L1) + 3 (L2) = 9
    // 但 anchor 在第 0 行, delta 应该只是 L0 的 3
    // 所以 delta 不是一个固定值!

    return 0;
}
