#ifndef CREDITRULES_H
#define CREDITRULES_H

#include <QString>
#include <QMap>

struct CreditRule {
    QString name;           // 规则名称
    int changeValue;        // 变更分值（正加负扣）
    int maxMonthly;         // 每月最大触发次数，0=不限
    bool enabled;           // 是否启用
};

class CreditRules
{
public:
    static CreditRules* instance();

    CreditRule getRule(const QString& ruleCode) const;
    bool isValidRule(const QString& ruleCode) const;
    int getMonthlyLimit(const QString& ruleCode) const;

private:
    CreditRules();
    void initRules();

    QMap<QString, CreditRule> m_rules;
    static CreditRules* m_instance;
};

#endif
