#pragma once

#include <QListWidget>
#include <QSignalBlocker>
#include <QVariantMap>

namespace genedot {
inline QVariantList listState(const QListWidget* selectionListWidget, bool isGroupList)
{
    QVariantList selectionState;
    if (!selectionListWidget)
    {
        return selectionState;
    }
    for (int itemIndex = 0; itemIndex < selectionListWidget->count(); ++itemIndex)
    {
        const auto* selectionItem = selectionListWidget->item(itemIndex);
        selectionState << QVariantMap{{"index", selectionItem->data(Qt::UserRole)}, {"name", selectionItem->text()}, {"active", isGroupList ? selectionItem->checkState() == Qt::Checked : selectionItem->isSelected()}};
    }
    return selectionState;
}

inline void restoreList(QListWidget* selectionListWidget, const QVariantList& savedSelectionState, bool isGroupList)
{
    if (savedSelectionState.isEmpty())
    {
        return;
    }

    const QSignalBlocker blocker(selectionListWidget);
    // Index AND name must match. A unique name can resolve reordered or
    // removed dimensions. Ambiguous duplicate names are never guessed.
    for (int itemIndex = 0; itemIndex < selectionListWidget->count(); ++itemIndex)
    {
        if (isGroupList)
        {
            selectionListWidget->item(itemIndex)->setCheckState(Qt::Unchecked);
        }
        else
        {
            selectionListWidget->item(itemIndex)->setSelected(false);
        }
    }
    int restoredRow = 0;
    for (const auto& savedItemState : savedSelectionState)
    {
        const auto savedItemVariantMap = savedItemState.toMap();
        QListWidgetItem* matchingItem  = nullptr;
        for (int itemIndex = restoredRow; itemIndex < selectionListWidget->count(); ++itemIndex)
        {
            auto* selectionItem = selectionListWidget->item(itemIndex);
            if (selectionItem->data(Qt::UserRole).toInt() == savedItemVariantMap.value("index").toInt() && selectionItem->text() == savedItemVariantMap.value("name").toString())
            {
                matchingItem = selectionItem;
                break;
            }
        }
        if (!matchingItem)
        {
            int matchingItemCount = 0;
            for (int itemIndex = restoredRow; itemIndex < selectionListWidget->count(); ++itemIndex)
            {
                if (selectionListWidget->item(itemIndex)->text() == savedItemVariantMap.value("name").toString())
                {
                    matchingItem = selectionListWidget->item(itemIndex);
                    ++matchingItemCount;
                }
            }
            if (matchingItemCount != 1)
            {
                matchingItem = nullptr;
            }
        }
        if (!matchingItem)
        {
            continue;
        }
        const bool isActive = savedItemVariantMap.value("active").toBool();
        const int itemRow   = selectionListWidget->row(matchingItem);
        if (itemRow != restoredRow)
        {
            selectionListWidget->insertItem(restoredRow, selectionListWidget->takeItem(itemRow));
        }
        if (isGroupList)
        {
            matchingItem->setCheckState(isActive ? Qt::Checked : Qt::Unchecked);
        }
        else
        {
            matchingItem->setSelected(isActive);
        }
        ++restoredRow;
    }
}
} // namespace genedot
