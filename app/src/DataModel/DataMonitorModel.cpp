#include "DataModel/DataMonitorModel.h"
#include "Misc/TimerEvents.h"
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QStandardPaths>
#include <QTextStream>
#include <QTime>
#include <QRegularExpression>
#include <QStringList>
#include <algorithm>

namespace {

constexpr auto SequenceKey = "__monitorSeq";

qint64 timestampMsecs(const QVariantMap &frame)
{
    const QString text = frame.value("timestamp").toString();
    QTime time = QTime::fromString(text, "hh:mm:ss.zzz");
    if (!time.isValid())
        time = QTime::fromString(text, "h:mm:ss.zzz");
    if (!time.isValid())
        time = QTime::fromString(text, "hh:mm:ss");
    if (!time.isValid())
        time = QTime::fromString(text, "h:mm:ss");
    if (!time.isValid())
        return -1;

    return QTime(0, 0).msecsTo(time);
}

bool frameTimestampLess(const QVariant &lhs, const QVariant &rhs)
{
    const QVariantMap left = lhs.toMap();
    const QVariantMap right = rhs.toMap();
    const qint64 leftTime = timestampMsecs(left);
    const qint64 rightTime = timestampMsecs(right);

    if (leftTime != rightTime) {
        if (leftTime < 0)
            return false;
        if (rightTime < 0)
            return true;
        return leftTime < rightTime;
    }

    return left.value(SequenceKey).toLongLong() < right.value(SequenceKey).toLongLong();
}

bool canAppendInTimestampOrder(const QVariantList &existing, const QList<QVariant> &incoming)
{
    if (existing.isEmpty() || incoming.isEmpty())
        return true;

    return !frameTimestampLess(incoming.first(), existing.last());
}

QString normalizedSearchText(QString text)
{
    text = text.trimmed();
    text.remove(QStringLiteral("0x"), Qt::CaseInsensitive);
    text.remove(QRegularExpression(QStringLiteral("[\\s,;:_-]+")));
    return text;
}

QString hexValue(qulonglong value, int minDigits)
{
    return QStringLiteral("0x%1").arg(QStringLiteral("%1").arg(value, minDigits, 16, QChar('0')).toUpper());
}

bool parseFrameNumber(const QString &text, qulonglong *value)
{
    QString cleaned = text.trimmed();
    cleaned.remove(QRegularExpression(QStringLiteral("[\\s,;:_]+")));
    if (cleaned.isEmpty())
        return false;

    bool ok = false;
    if (cleaned.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        cleaned.remove(0, 2);
        *value = cleaned.toULongLong(&ok, 16);
    } else {
        *value = cleaned.toULongLong(&ok, 10);
    }
    return ok;
}

QString canonicalFieldName(QString field)
{
    field = field.trimmed().toLower();

    if (field == "src")
        return QStringLiteral("sender");
    if (field == "dst")
        return QStringLiteral("receiver");
    if (field == "len")
        return QStringLiteral("len");
    if (field == "type")
        return QStringLiteral("type");
    if (field == "seq")
        return QStringLiteral("seq");
    if (field == "cmdset" || field == "cmd_set" || field == "set")
        return QStringLiteral("cmdSet");
    if (field == "cmdid" || field == "cmd_id" || field == "id")
        return QStringLiteral("cmdId");
    if (field == "data")
        return QStringLiteral("data");

    return {};
}

QString frameValueForKey(const QVariantMap &frame, const QString &key)
{
    if (key == QStringLiteral("len"))
        return frame.value(QStringLiteral("len")).toString();

    if (key == QStringLiteral("seq")) {
        qulonglong value = 0;
        if (parseFrameNumber(frame.value(QStringLiteral("seq")).toString(), &value))
            return hexValue(value, 4);
        return {};
    }

    return frame.value(key).toString();
}

QString frameValueForField(const QVariantMap &frame, const QString &field)
{
    const QString key = canonicalFieldName(field);
    if (key.isEmpty())
        return {};
    return frameValueForKey(frame, key);
}

QStringList monitorFilterKeys()
{
    return {
        QStringLiteral("sender"),
        QStringLiteral("receiver"),
        QStringLiteral("len"),
        QStringLiteral("type"),
        QStringLiteral("seq"),
        QStringLiteral("cmdSet"),
        QStringLiteral("cmdId"),
        QStringLiteral("data"),
    };
}

QString safeFilePart(QString text, const QString &fallback)
{
    text = text.trimmed();
    if (text.isEmpty())
        text = fallback;
    text.replace(QRegularExpression(QStringLiteral("[<>:\"/\\\\|?*\\x00-\\x1F]+")), QStringLiteral("_"));
    text.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral("_"));
    text.replace(QRegularExpression(QStringLiteral("_+")), QStringLiteral("_"));
    text = text.trimmed();
    while (text.startsWith(QLatin1Char('_')))
        text.remove(0, 1);
    while (text.endsWith(QLatin1Char('_')))
        text.chop(1);
    if (text.isEmpty())
        text = fallback;
    return text.left(96);
}

QString compactTimestamp(QString timestamp)
{
    timestamp = timestamp.trimmed();
    timestamp.remove(QRegularExpression(QStringLiteral("[^0-9A-Za-z]+")));
    return timestamp.isEmpty() ? QStringLiteral("unknown") : timestamp;
}

QString formatHexBytes(QString rawHex)
{
    rawHex.remove(QStringLiteral("0x"), Qt::CaseInsensitive);
    rawHex.replace(QRegularExpression(QStringLiteral("[^0-9A-Fa-f]+")), QStringLiteral(" "));

    QStringList output;
    const QStringList groups = rawHex.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (QString group : groups) {
        if (group.size() % 2 != 0)
            group.prepend(QLatin1Char('0'));
        for (int i = 0; i + 1 < group.size(); i += 2)
            output.append(QStringLiteral("0x%1").arg(group.mid(i, 2).toUpper()));
    }
    return output.join(QLatin1Char(' '));
}

QString frameRawHex(const QVariantMap &frame)
{
    QString rawHex = frame.value(QStringLiteral("rawHex")).toString().trimmed();
    if (rawHex.isEmpty())
        rawHex = frame.value(QStringLiteral("data")).toString().trimmed();
    return formatHexBytes(rawHex);
}

bool wildcardMatchOne(const QString &candidate, QString pattern)
{
    pattern = pattern.trimmed();
    if ((pattern.startsWith('"') && pattern.endsWith('"'))
        || (pattern.startsWith('\'') && pattern.endsWith('\''))) {
        pattern = pattern.mid(1, pattern.size() - 2);
    }

    if (pattern.isEmpty())
        return candidate.isEmpty();

    if (!pattern.contains('*') && !pattern.contains('?'))
        return candidate.contains(pattern, Qt::CaseInsensitive);

    const QString regexPattern = QRegularExpression::wildcardToRegularExpression(pattern);
    const QRegularExpression regex(regexPattern, QRegularExpression::CaseInsensitiveOption);
    return regex.match(candidate).hasMatch();
}

bool wildcardMatch(const QString &candidate, const QString &pattern)
{
    if (wildcardMatchOne(candidate, pattern))
        return true;

    const QString normalizedCandidate = normalizedSearchText(candidate);
    const QString normalizedPattern = normalizedSearchText(pattern);
    if (normalizedCandidate == candidate && normalizedPattern == pattern)
        return false;

    return wildcardMatchOne(normalizedCandidate, normalizedPattern);
}

QStringList splitLogic(const QString &text, const QString &op)
{
    QStringList parts;
    int start = 0;
    while (start <= text.size()) {
        const int pos = text.indexOf(op, start, Qt::CaseInsensitive);
        const QString part = pos < 0 ? text.mid(start) : text.mid(start, pos - start);
        if (!part.trimmed().isEmpty())
            parts.append(part.trimmed());
        if (pos < 0)
            break;
        start = pos + op.size();
    }
    return parts;
}

QString trimOptionalParens(QString text)
{
    text = text.trimmed();
    while (text.startsWith('(') && text.endsWith(')') && text.size() >= 2)
        text = text.mid(1, text.size() - 2).trimmed();
    return text;
}

bool matchTerm(const QVariantMap &frame, QString term)
{
    term = trimOptionalParens(term);
    if (term.isEmpty())
        return true;

    int separator = term.indexOf(':');
    const int equalPos = term.indexOf('=');
    if (separator < 0 || (equalPos >= 0 && equalPos < separator))
        separator = equalPos;

    if (separator >= 0) {
        const QString field = term.left(separator).trimmed();
        const QString pattern = term.mid(separator + 1).trimmed();
        if (field.isEmpty())
            return false;
        return wildcardMatch(frameValueForField(frame, field), pattern);
    }

    const QStringList keys = monitorFilterKeys();
    for (const QString &key : keys) {
        if (wildcardMatch(frameValueForKey(frame, key), term))
            return true;
    }

    return false;
}

bool matchFilterExpression(const QVariantMap &frame, const QString &expression)
{
    QString normalized = expression;
    normalized.replace(';', QStringLiteral("&&"));

    const QStringList orGroups = splitLogic(normalized, QStringLiteral("||"));
    for (const QString &group : orGroups) {
        bool groupMatches = true;
        const QStringList terms = splitLogic(group, QStringLiteral("&&"));
        for (const QString &term : terms) {
            if (!matchTerm(frame, term)) {
                groupMatches = false;
                break;
            }
        }
        if (groupMatches)
            return true;
    }

    return orGroups.isEmpty();
}

} // namespace

DataMonitorModel::DataMonitorModel(QObject *parent)
    : QAbstractListModel(parent)
{
    connect(&Misc::TimerEvents::instance(), &Misc::TimerEvents::uiTimeout,
            this, &DataMonitorModel::refreshFromQueue);
}

int DataMonitorModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    if (!m_filterText.isEmpty()) return m_filteredIndices.size();
    return m_allFrames.size();
}

QVariant DataMonitorModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) return QVariant();

    int row = index.row();
    int actualIndex;

    if (!m_filterText.isEmpty()) {
        if (row >= m_filteredIndices.size()) return QVariant();
        actualIndex = m_filteredIndices[row];
    } else {
        actualIndex = row;
    }

    if (actualIndex >= m_allFrames.size()) return QVariant();

    const QVariantMap &frame = m_allFrames.at(actualIndex).toMap();

    switch (role) {
    case TimestampRole: return frame.value("timestamp", "");
    case DirectionRole: return frame.value("direction", "↓");
    case SenderRole:    return frame.value("sender", "");
    case ReceiverRole:  return frame.value("receiver", "");
    case LenRole:       return frame.value("len", "");
    case TypeRole:      return frame.value("type", "");
    case SeqRole:       return frame.value("seq", "");
    case CmdIdRole:     return frame.value("cmdId", "");
    case CmdSetRole:    return frame.value("cmdSet", "");
    case DataRole:      return frame.value("data", "");
    case CrcRole:       return frame.value("crc", "");
    case Crc8Role:      return frame.value("crc8", "");
    case RawHexRole:    return frame.value("rawHex", "");
    case RowColorRole:  return frame.value("rowColor", frame.value("color", frame.value("bgColor", "")));
    case TextColorRole: return frame.value("textColor", "");
    }

    return QVariant();
}

QVariantMap DataMonitorModel::frameAt(int row) const
{
    if (row < 0)
        return {};

    int actualIndex = row;
    if (!m_filterText.isEmpty()) {
        if (row >= m_filteredIndices.size())
            return {};
        actualIndex = m_filteredIndices.at(row);
    }

    if (actualIndex < 0 || actualIndex >= m_allFrames.size())
        return {};

    QVariantMap frame = m_allFrames.at(actualIndex).toMap();
    frame.remove(SequenceKey);
    frame.insert("row", row);
    return frame;
}

QVariantList DataMonitorModel::frames(int maxCount) const
{
    QVariantList result;
    const int total = m_allFrames.size();
    const int start = maxCount > 0 ? qMax(0, total - maxCount) : 0;

    result.reserve(total - start);
    for (int i = start; i < total; ++i) {
        QVariantMap frame = m_allFrames.at(i).toMap();
        frame.remove(SequenceKey);
        frame.insert(QStringLiteral("row"), i);
        result.append(frame);
    }

    return result;
}

bool DataMonitorModel::exportRawHex(const QString &linkType, const QString &linkInfo)
{
    QVariantList snapshot = m_allFrames;
    {
        QMutexLocker lock(&m_mutex);
        for (const QVariantMap &frame : m_displayQueue)
            snapshot.append(frame);
    }

    if (snapshot.isEmpty()) {
        emit exportFailed(QStringLiteral("没有可导出的协议监控数据"));
        return false;
    }

    std::stable_sort(snapshot.begin(), snapshot.end(), frameTimestampLess);

    const QVariantMap firstFrame = snapshot.first().toMap();
    const QVariantMap lastFrame = snapshot.last().toMap();
    const QString startTime = compactTimestamp(firstFrame.value(QStringLiteral("timestamp")).toString());
    const QString endTime = compactTimestamp(lastFrame.value(QStringLiteral("timestamp")).toString());
    const QString datePart = QDate::currentDate().toString(QStringLiteral("yyyyMMdd"));
    const QString linkPart = safeFilePart(QStringList{
        linkType.trimmed().isEmpty() ? QStringLiteral("LINK") : linkType.trimmed(),
        linkInfo.trimmed()
    }.join(QLatin1Char('_')), QStringLiteral("LINK"));
    const QString defaultName = QStringLiteral("KPtools_%1_%2_%3-%4.txt")
        .arg(linkPart, datePart, startTime, endTime);

    QString startDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (startDir.isEmpty())
        startDir = QDir::homePath();
    const QString path = QFileDialog::getSaveFileName(nullptr,
                                                      QStringLiteral("导出原始十六进制数据"),
                                                      QDir(startDir).filePath(defaultName),
                                                      QStringLiteral("Text files (*.txt);;All files (*.*)"));
    if (path.isEmpty())
        return false;

    QString outputPath = path;
    if (!outputPath.endsWith(QStringLiteral(".txt"), Qt::CaseInsensitive))
        outputPath += QStringLiteral(".txt");

    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit exportFailed(QStringLiteral("无法写入文件: %1").arg(outputPath));
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    for (const QVariant &item : snapshot) {
        const QVariantMap frame = item.toMap();
        const QString rawHex = frameRawHex(frame);
        if (rawHex.isEmpty())
            continue;
        const QString timestamp = frame.value(QStringLiteral("timestamp")).toString().trimmed();
        const QString direction = frame.value(QStringLiteral("direction")).toString().trimmed();
        stream << (timestamp.isEmpty() ? QStringLiteral("-") : timestamp)
               << QLatin1Char(' ')
               << (direction.isEmpty() ? QStringLiteral("-") : direction)
               << QLatin1Char(' ')
               << rawHex
               << Qt::endl;
    }

    emit exportFinished(QDir::toNativeSeparators(outputPath));
    return true;
}

QHash<int, QByteArray> DataMonitorModel::roleNames() const
{
    QHash<int, QByteArray> names;
    names[TimestampRole] = "timestamp";
    names[DirectionRole] = "direction";
    names[SenderRole]    = "sender";
    names[ReceiverRole]  = "receiver";
    names[LenRole]       = "len";
    names[TypeRole]      = "type";
    names[SeqRole]       = "seq";
    names[CmdIdRole]     = "cmdId";
    names[CmdSetRole]    = "cmdSet";
    names[DataRole]      = "data";
    names[CrcRole]       = "crc";
    names[Crc8Role]      = "crc8";
    names[RawHexRole]    = "rawHex";
    names[RowColorRole]  = "rowColor";
    names[TextColorRole] = "textColor";
    return names;
}

void DataMonitorModel::setPaused(bool paused)
{
    if (m_paused != paused) {
        m_paused = paused;
        emit pausedChanged();
    }
}

void DataMonitorModel::setFilterText(const QString &text)
{
    if (m_filterText != text) {
        beginResetModel();
        m_filterText = text;
        rebuildFilterIndices();
        endResetModel();
        emit filterTextChanged();
        emit countChanged();
    }
}

void DataMonitorModel::setRefreshIntervalMs(int ms)
{
    if (m_refreshInterval != ms && ms > 0) {
        m_refreshInterval = ms;
        // UI 刷新已由 TimerEvents 统一管理，此属性保留供未来扩展
        emit refreshIntervalChanged();
    }
}

void DataMonitorModel::setCommandEchoEnabled(bool enabled)
{
    if (m_commandEchoEnabled == enabled)
        return;

    m_commandEchoEnabled = enabled;
    emit commandEchoEnabledChanged();
}

void DataMonitorModel::addFrame(const QVariantMap &frame)
{
    if (m_paused) return;

    appendFramesSorted({frame});
}

void DataMonitorModel::enqueueFrame(const QVariantMap &frame)
{
    QMutexLocker lock(&m_mutex);
    m_displayQueue.enqueue(frame);
}

void DataMonitorModel::enqueueFrames(const QList<QVariantMap> &frames)
{
    if (frames.isEmpty())
        return;

    QMutexLocker lock(&m_mutex);
    for (const QVariantMap &frame : frames)
        m_displayQueue.enqueue(frame);
}

void DataMonitorModel::clear()
{
    beginResetModel();
    m_allFrames.clear();
    m_filteredIndices.clear();
    m_nextSequence = 0;
    endResetModel();
    emit countChanged();
}

void DataMonitorModel::refreshFromQueue()
{
    if (m_paused) return;

    // 取出队列中的帧
    QList<QVariantMap> frames;
    {
        QMutexLocker lock(&m_mutex);
        while (!m_displayQueue.isEmpty()) {
            frames.append(m_displayQueue.dequeue());
        }
    }

    if (frames.isEmpty()) return;

    appendFramesSorted(frames);
}

void DataMonitorModel::appendFramesSorted(const QList<QVariantMap> &frames)
{
    if (frames.isEmpty())
        return;

    QList<QVariant> preparedFrames;
    preparedFrames.reserve(frames.size());

    for (QVariantMap frame : frames) {
        frame[SequenceKey] = m_nextSequence++;
        preparedFrames.append(frame);
    }

    std::stable_sort(preparedFrames.begin(), preparedFrames.end(), frameTimestampLess);

    const bool canFastAppend = m_filterText.isEmpty()
        && canAppendInTimestampOrder(m_allFrames, preparedFrames)
        && (m_allFrames.size() + preparedFrames.size() - MAX_FRAMES <= m_allFrames.size());

    if (canFastAppend) {
        const int overflow = qMax(0, m_allFrames.size() + preparedFrames.size() - MAX_FRAMES);
        if (overflow > 0) {
            beginRemoveRows(QModelIndex(), 0, overflow - 1);
            for (int i = 0; i < overflow; ++i)
                m_allFrames.removeFirst();
            endRemoveRows();
        }

        const int firstRow = m_allFrames.size();
        const int lastRow = firstRow + preparedFrames.size() - 1;
        beginInsertRows(QModelIndex(), firstRow, lastRow);
        for (const QVariant &frame : preparedFrames)
            m_allFrames.append(frame);
        endInsertRows();

        emit countChanged();
        return;
    }

    beginResetModel();

    for (const QVariant &frame : preparedFrames)
        m_allFrames.append(frame);

    std::stable_sort(m_allFrames.begin(), m_allFrames.end(), frameTimestampLess);

    while (m_allFrames.size() > MAX_FRAMES)
        m_allFrames.removeFirst();

    rebuildFilterIndices();

    endResetModel();
    emit countChanged();
}

bool DataMonitorModel::matchesFilter(const QVariantMap &frame) const
{
    if (m_filterText.isEmpty())
        return true;

    return matchFilterExpression(frame, m_filterText.trimmed());
}

void DataMonitorModel::rebuildFilterIndices()
{
    m_filteredIndices.clear();
    if (m_filterText.isEmpty()) return;

    for (int i = 0; i < m_allFrames.size(); ++i) {
        if (matchesFilter(m_allFrames.at(i).toMap())) {
            m_filteredIndices.append(i);
        }
    }
}
