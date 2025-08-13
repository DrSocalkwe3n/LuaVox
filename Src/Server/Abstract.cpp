#include "Abstract.hpp"


namespace LV::Server {

NodeStateCondition nodestateExpression(const std::vector<NodestateEntry>& entries, const std::string& expression) {
    // Скомпилировать выражение и просчитать таблицу CT

    std::unordered_map<std::string, int> valueToInt;
    for(const NodestateEntry& entry : entries) {
        for(size_t index = 0; index < entry.ValueNames.size(); index++) {
            valueToInt[entry.ValueNames[index]] = index;
        }
    }

    // Парсинг токенов
    enum class EnumTokenKind {
        LParen, RParen,
        Plus, Minus, Star, Slash, Percent,
        Not, And, Or,
        LT, LE, GT, GE, EQ, NE
    };

    std::vector<std::variant<EnumTokenKind, std::string, int, uint16_t>> tokens;
    ssize_t pos = 0;
    auto skipWS = [&](){ while(pos<expression.size() && std::isspace((unsigned char) expression[pos])) ++pos; };

    for(; pos < expression.size(); pos++) {
        skipWS();

        char c = expression[pos];

        // Числа
        if(std::isdigit(c)) {
            ssize_t npos = pos;
            for(; npos < expression.size() && std::isdigit(expression[npos]); npos++);
            int value = std::stoi(expression.substr(pos, npos-pos));
            tokens.push_back(value);
            continue;
        }

        // Переменные
        if(std::isalpha(c)) {
            ssize_t npos = pos;
            for(; npos < expression.size() && std::isalpha(expression[npos]); npos++);
            std::string value = expression.substr(pos, npos-pos);
            if(value == "true")
                tokens.push_back(1);
            else if(value == "false")
                tokens.push_back(0);
            else
                tokens.push_back(value);
            continue;
        }

        // Двойные операторы
        if(pos-1 < expression.size()) {
            char n = expression[pos+1];

            if(c == '<' && n == '=') {
                tokens.push_back(EnumTokenKind::LE);
                pos++;
                continue;
            } else if(c == '>' && n == '=') {
                tokens.push_back(EnumTokenKind::GE);
                pos++;
                continue;
            } else if(c == '=' && n == '=') {
                tokens.push_back(EnumTokenKind::EQ);
                pos++;
                continue;
            } else if(c == '!' && n == '=') {
                tokens.push_back(EnumTokenKind::NE);
                pos++;
                continue;
            }
        }

        // Операторы
        switch(c) {
            case '(': tokens.push_back(EnumTokenKind::LParen);
            case ')': tokens.push_back(EnumTokenKind::RParen);
            case '+': tokens.push_back(EnumTokenKind::Plus);
            case '-': tokens.push_back(EnumTokenKind::Minus);
            case '*': tokens.push_back(EnumTokenKind::Star);
            case '/': tokens.push_back(EnumTokenKind::Slash);
            case '%': tokens.push_back(EnumTokenKind::Percent);
            case '!': tokens.push_back(EnumTokenKind::Not);
            case '&': tokens.push_back(EnumTokenKind::And);
            case '|': tokens.push_back(EnumTokenKind::Or);
            case '<': tokens.push_back(EnumTokenKind::LT);
            case '>': tokens.push_back(EnumTokenKind::GT);
        }

        MAKE_ERROR("Недопустимый символ: " << c);
    }

    // Разбор токенов
    enum class Op {
        Add, Sub, Mul, Div, Mod,
        LT, LE, GT, GE, EQ, NE,
        And, Or,
        Pos, Neg, Not
    };

    struct Node {
        struct Num { int v; };
        struct Var { std::string name; };
        struct Unary { Op op; uint16_t rhs; };
        struct Binary { Op op; uint16_t lhs, rhs; };
        std::variant<Num, Var, Unary, Binary> v;
    };

    std::vector<Node> nodes;

    for(size_t index = 0; index < tokens.size(); index++) {
        auto &token = tokens[index];

        if(std::string* value = std::get_if<std::string>(&token)) {
            if(*value == "false") {
                token = 0;
            } else if(*value == "true") {
                token = 1;
            } else if(auto iter = valueToInt.find(*value); iter != valueToInt.end()) {
                token = iter->second; // TODO:
            } else {
                Node node;
                node.v = Node::Var(*value);
                nodes.emplace_back(std::move(node));
                assert(nodes.size() < std::pow(2, 16)-64);
                token = uint16_t(nodes.size()-1);
            }
        }
    }

    // Рекурсивный разбор выражений в скобках
    std::function<uint16_t(size_t pos)> lambdaParse = [&](size_t pos) -> uint16_t {
        size_t end = tokens.size();
        
        // Парсим выражения в скобках
        for(size_t index = pos; index < tokens.size(); index++) {
            if(EnumTokenKind* kind = std::get_if<EnumTokenKind>(&tokens[index])) {
                if(*kind == EnumTokenKind::LParen) {
                    uint16_t node = lambdaParse(index+1);
                    tokens.insert(tokens.begin()+index, node);
                    tokens.erase(tokens.begin()+index+1, tokens.begin()+index+3);
                    end = tokens.size();
                } else if(*kind == EnumTokenKind::RParen) {
                    end = index;
                    break;
                }
            }
        }

        // Обрабатываем унарные операции
        for(ssize_t index = end; index >= pos; index--) {
            if(EnumTokenKind *kind = std::get_if<EnumTokenKind>(&tokens[index])) {
                if(*kind != EnumTokenKind::Not && *kind != EnumTokenKind::Plus && *kind != EnumTokenKind::Minus)
                    continue;

                if(index+1 >= end)
                    MAKE_ERROR("Отсутствует операнд");

                auto rightToken = tokens[index+1];
                if(std::holds_alternative<EnumTokenKind>(rightToken))
                    MAKE_ERROR("Недопустимый операнд");

                if(int* value = std::get_if<int>(&rightToken)) {
                    if(*kind == EnumTokenKind::Not)
                        tokens[index] = *value ? 0 : 1;
                    else if(*kind == EnumTokenKind::Plus)
                        tokens[index] = +*value;
                    else if(*kind == EnumTokenKind::Minus)
                        tokens[index] = -*value;

                } else if(uint16_t* value = std::get_if<uint16_t>(&rightToken)) {
                    Node node;
                    Node::Unary un;
                    un.rhs = *value;

                    if(*kind == EnumTokenKind::Not)
                        un.op = Op::Not;
                    else if(*kind == EnumTokenKind::Plus)
                        un.op = Op::Pos;
                    else if(*kind == EnumTokenKind::Minus)
                        un.op = Op::Neg;

                    node.v = un;
                    nodes.emplace_back(std::move(node));
                    assert(nodes.size() < std::pow(2, 16)-64);
                    tokens[index] = uint16_t(nodes.size()-1);
                }

                end--;
                tokens.erase(tokens.begin()+index+1);
            }
        }

        // Бинарные в порядке приоритета
        for(int priority = 0; priority < 6; priority++)
            for(size_t index = pos; index < end; index++) {
                EnumTokenKind *kind = std::get_if<EnumTokenKind>(&tokens[index]);

                if(!kind)
                    continue;

                if(priority == 0 && *kind != EnumTokenKind::Star && *kind != EnumTokenKind::Slash && *kind != EnumTokenKind::Percent)
                    continue;
                if(priority == 1 && *kind != EnumTokenKind::Plus && *kind != EnumTokenKind::Minus)
                    continue;
                if(priority == 2 && *kind != EnumTokenKind::LT && *kind != EnumTokenKind::GT && *kind != EnumTokenKind::LE && *kind != EnumTokenKind::GE)
                    continue;
                if(priority == 3 && *kind != EnumTokenKind::EQ && *kind != EnumTokenKind::NE)
                    continue;
                if(priority == 4 && *kind != EnumTokenKind::And)
                    continue;
                if(priority == 5 && *kind != EnumTokenKind::Or)
                    continue;

                if(index == pos)
                    MAKE_ERROR("Отсутствует операнд перед");
                else if(index == end-1)
                    MAKE_ERROR("Отсутствует операнд после");

                auto &leftToken = tokens[index-1];
                auto &rightToken = tokens[index+1];

                if(std::holds_alternative<EnumTokenKind>(leftToken))
                    MAKE_ERROR("Недопустимый операнд");

                if(std::holds_alternative<EnumTokenKind>(rightToken))
                    MAKE_ERROR("Недопустимый операнд");

                if(std::holds_alternative<int>(leftToken) && std::holds_alternative<int>(rightToken)) {
                    int value;

                    switch(*kind) {
                    case EnumTokenKind::Plus:       value = std::get<int>(leftToken) +  std::get<int>(rightToken); break;
                    case EnumTokenKind::Minus:      value = std::get<int>(leftToken) -  std::get<int>(rightToken); break;
                    case EnumTokenKind::Star:       value = std::get<int>(leftToken) *  std::get<int>(rightToken); break;
                    case EnumTokenKind::Slash:      value = std::get<int>(leftToken) /  std::get<int>(rightToken); break;
                    case EnumTokenKind::Percent:    value = std::get<int>(leftToken) %  std::get<int>(rightToken); break;
                    case EnumTokenKind::And:        value = std::get<int>(leftToken) && std::get<int>(rightToken); break;
                    case EnumTokenKind::Or:         value = std::get<int>(leftToken) || std::get<int>(rightToken); break;
                    case EnumTokenKind::LT:         value = std::get<int>(leftToken) <  std::get<int>(rightToken); break;
                    case EnumTokenKind::LE:         value = std::get<int>(leftToken) <= std::get<int>(rightToken); break;
                    case EnumTokenKind::GT:         value = std::get<int>(leftToken) >  std::get<int>(rightToken); break;
                    case EnumTokenKind::GE:         value = std::get<int>(leftToken) >= std::get<int>(rightToken); break;
                    case EnumTokenKind::EQ:         value = std::get<int>(leftToken) == std::get<int>(rightToken); break;
                    case EnumTokenKind::NE:         value = std::get<int>(leftToken) != std::get<int>(rightToken); break;

                    default: assert(false);
                    }


                    leftToken = value;
                } else {
                    Node node;
                    Node::Binary bin;

                    switch(*kind) {
                    case EnumTokenKind::Plus:       bin.op = Op::Add; break;
                    case EnumTokenKind::Minus:      bin.op = Op::Sub; break;
                    case EnumTokenKind::Star:       bin.op = Op::Mul; break;
                    case EnumTokenKind::Slash:      bin.op = Op::Div; break;
                    case EnumTokenKind::Percent:    bin.op = Op::Mod; break;
                    case EnumTokenKind::And:        bin.op = Op::And; break;
                    case EnumTokenKind::Or:         bin.op = Op::Or;  break;
                    case EnumTokenKind::LT:         bin.op = Op::LT;  break;
                    case EnumTokenKind::LE:         bin.op = Op::LE;  break;
                    case EnumTokenKind::GT:         bin.op = Op::GT;  break;
                    case EnumTokenKind::GE:         bin.op = Op::GE;  break;
                    case EnumTokenKind::EQ:         bin.op = Op::EQ;  break;
                    case EnumTokenKind::NE:         bin.op = Op::NE;  break;

                    default: assert(false);
                    }

                    if(int* value = std::get_if<int>(&leftToken)) {
                        Node valueNode;
                        valueNode.v = Node::Num(*value);
                        nodes.emplace_back(std::move(valueNode));
                        assert(nodes.size() < std::pow(2, 16)-64);
                        bin.lhs = uint16_t(nodes.size()-1);
                    } else if(uint16_t* nodeId = std::get_if<uint16_t>(&leftToken)) {
                        bin.lhs = *nodeId;
                    }

                    if(int* value = std::get_if<int>(&rightToken)) {
                        Node valueNode;
                        valueNode.v = Node::Num(*value);
                        nodes.emplace_back(std::move(valueNode));
                        assert(nodes.size() < std::pow(2, 16)-64);
                        bin.rhs = uint16_t(nodes.size()-1);
                    } else if(uint16_t* nodeId = std::get_if<uint16_t>(&rightToken)) {
                        bin.rhs = *nodeId;
                    }

                    nodes.emplace_back(std::move(node));
                    assert(nodes.size() < std::pow(2, 16)-64);
                    leftToken = uint16_t(nodes.size()-1);
                }

                tokens.erase(tokens.begin()+index, tokens.begin()+index+2);
                end -= 2;
                index--;
            }

        if(tokens.size() != 1)
            MAKE_ERROR("Выражение не корректно");

        if(uint16_t* nodeId = std::get_if<uint16_t>(&tokens[0])) {
            return *nodeId;
        } else if(int* value = std::get_if<int>(&tokens[0])) {
            Node node;
            node.v = Node::Num(*value);
            nodes.emplace_back(std::move(node));
            assert(nodes.size() < std::pow(2, 16)-64);
            return uint16_t(nodes.size()-1);
        } else {
            MAKE_ERROR("Выражение не корректно");
        }
    };

    uint16_t nodeId = lambdaParse(0);
    if(!tokens.empty())
        MAKE_ERROR("Выражение не действительно");

    std::unordered_map<std::string, int> vars;
    std::function<int(uint16_t)> lambdaCalcNode = [&](uint16_t nodeId) -> int {
        const Node& node = nodes[nodeId];
        if(const Node::Num* value = std::get_if<Node::Num>(&node.v)) {
            return value->v;
        } else if(const Node::Var* value = std::get_if<Node::Var>(&node.v)) {
            auto iter = vars.find(value->name);
            if(iter == vars.end())
                MAKE_ERROR("Неопознанное состояние");

            return iter->second;
        } else if(const Node::Unary* value = std::get_if<Node::Unary>(&node.v)) {
            int rNodeValue = lambdaCalcNode(value->rhs);
            switch(value->op) {
            case Op::Not: return !rNodeValue;
            case Op::Pos: return +rNodeValue;
            case Op::Neg: return -rNodeValue;
            default:
                assert(false);
            }
        } else if(const Node::Binary* value = std::get_if<Node::Binary>(&node.v)) {
            int lNodeValue = lambdaCalcNode(value->lhs);
            int rNodeValue = lambdaCalcNode(value->rhs);

            switch(value->op) {
            case Op::Add: return lNodeValue+rNodeValue;
            case Op::Sub: return lNodeValue-rNodeValue;
            case Op::Mul: return lNodeValue*rNodeValue;
            case Op::Div: return lNodeValue/rNodeValue;
            case Op::Mod: return lNodeValue%rNodeValue;
            case Op::LT:  return lNodeValue<rNodeValue;
            case Op::LE:  return lNodeValue<=rNodeValue;
            case Op::GT:  return lNodeValue>rNodeValue;
            case Op::GE:  return lNodeValue>=rNodeValue;
            case Op::EQ:  return lNodeValue==rNodeValue;
            case Op::NE:  return lNodeValue!=rNodeValue;
            case Op::And: return lNodeValue&&rNodeValue;
            case Op::Or:  return lNodeValue||rNodeValue;
            default:
                assert(false);
            }
        } else {
            assert(false);
        }
    };

    NodeStateCondition ct;
    for(int meta = 0; meta < 256; meta++) {
        int meta_temp = meta;

        for(size_t index = 0; index < entries.size(); index++) {
            const auto& entry = entries[index];

            vars[entry.Name] = meta_temp % entry.Variability;
            meta_temp /= entry.Variability;
        }

        ct[meta] = (bool) lambdaCalcNode(nodeId);
    }

    return ct;
}
}

namespace std {

template <>
struct hash<LV::Server::ServerObjectPos> {
    std::size_t operator()(const LV::Server::ServerObjectPos& obj) const {
        return std::hash<uint32_t>()(obj.WorldId) ^ std::hash<LV::Pos::Object>()(obj.ObjectPos); 
    } 
};
}