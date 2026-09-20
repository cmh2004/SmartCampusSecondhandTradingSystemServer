// creditrules.cpp
#include "creditrules.h"

CreditRules* CreditRules::m_instance = nullptr;

CreditRules::CreditRules()
{
    initRules();
}

CreditRules* CreditRules::instance()
{
    if (!m_instance) {
        m_instance = new CreditRules();
    }
    return m_instance;
}

void CreditRules::initRules()
{
    // 纠纷判责（卖家责任）可动态传入分值，这里给默认值
    m_rules["DISPUTE_SELLER"] = {"纠纷判责（卖家责任）", -15, 0, true};
    m_rules["DISPUTE_BUYER"]  = {"纠纷判责（买家责任）", -8, 0, true};
    m_rules["REPORT_GOODS"]   = {"举报属实（商品违规）", -5, 0, true};
    m_rules["REPORT_USER"]    = {"举报属实（用户违规）", -10, 0, true};
    m_rules["REPORT_VALID"]   = {"举报属实（举报人）", 2, 0, true};
    m_rules["ORDER_TIMEOUT"]  = {"订单超时未支付（买家）", -2, 0, true};
    m_rules["REPORT_VALID"]   = {"举报属实（举报人）", 2, 0, true};
    m_rules["NO_VIOLATION_30D"] = {"连续30天无违规", 2, 0, true};
}

CreditRule CreditRules::getRule(const QString& ruleCode) const
{
    return m_rules.value(ruleCode, CreditRule{"", 0, 0, false});
}

bool CreditRules::isValidRule(const QString& ruleCode) const
{
    return m_rules.contains(ruleCode) && m_rules[ruleCode].enabled;
}

int CreditRules::getMonthlyLimit(const QString& ruleCode) const
{
    return m_rules.value(ruleCode).maxMonthly;
}
