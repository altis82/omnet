//
// This file is part of an OMNeT++/OMNEST simulation example.
//
// Copyright (C) 2003 Ahmet Sekercioglu
// Copyright (C) 2003-2015 Andras Varga
//
// This file is distributed WITHOUT ANY WARRANTY. See the file
// `license' for details on this and other legal matters.
//
#include <fstream>
#include <string.h>
#include <omnetpp.h>
using namespace omnetpp;

/**
 * In this class we add some debug messages to Txc1. When you run the
 * simulation in the OMNeT++ GUI Tkenv, the output will appear in
 * the main text window, and you can also open separate output windows
 * for `tic' and `toc'.
 */

class XMLWriter
{
private:
    std::string indent;
    std::ostream& out;
    bool tagOpen;

public:
    XMLWriter(std::ostream& out_) : out(out_) {
        tagOpen = false;
    }

    void openTag(const char *tagName) {
        if (tagOpen)
            out << ">\n"; // close previous
        out << indent << "<" << tagName;
        tagOpen = true;
        indent += "    ";
    }

    void writeAttr(const char *attrName, const char *value) {
        ASSERT(tagOpen);
        out << " " << attrName << "=\"" << xmlquote(value) << "\"";
    }

    void writeAttr(const char *attrName, std::string value) {
        ASSERT(tagOpen);
        out << " " << attrName << "=\"" << xmlquote(value) << "\"";
    }

    void writeAttr(const char *attrName, int value) {
        ASSERT(tagOpen);
        out << " " << attrName << "=\"" << value << "\"";
    }

    void writeAttr(const char *attrName, bool value) {
        ASSERT(tagOpen);
        out << " " << attrName << "=\"" << (value?"true":"false") << "\"";
    }

    void closeTag(const char *tagName) {
        indent = indent.substr(4);
        if (tagOpen)
            out << "/>\n";
        else
            out << indent << "</" << tagName << ">\n";
        tagOpen = false;
    }

    std::string xmlquote(const std::string& str) {
        if (!strchr(str.c_str(), '<') && !strchr(str.c_str(), '>') && !strchr(str.c_str(), '"') && !strchr(str.c_str(), '&'))
            return str;

        std::stringstream out;
        for (const char *s=str.c_str(); *s; s++)
        {
            switch (*s) {
            case '<': out << "&lt;"; break;
            case '>': out << "&gt;"; break;
            case '"': out << "&quot;"; break;
            case '&': out << "&amp;"; break;
            default: out << *s;
            }
        }
        return out.str();
    }
};


class Txc2 : public cSimpleModule
{
  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void dump(const char *filename);
    virtual void dumpComponent(XMLWriter& xml, cComponent *component);
    virtual void dumpProperties(XMLWriter& xml, cProperties *properties);
    virtual std::string quote(const char *propertyValue);
};

inline const char *replace(const char *orig, const char *what, const char *replacement) {
    return strcmp(orig,what)==0 ? replacement : orig;
}
std::string Txc2::quote(const char *propertyValue)
{
    if (!strchr(propertyValue, ',') && !strchr(propertyValue, ';'))
        return propertyValue;
    return std::string("\"") + propertyValue + "\"";
}

Define_Module(Txc2);
void Txc2::dumpProperties(XMLWriter& xml, cProperties *properties)
{
    for (int i = 0; i < properties->getNumProperties(); i++) {
        cProperty *prop = properties->get(i);
        xml.openTag("property");
        xml.writeAttr("name", prop->getFullName());
        std::string value;
        for (int k = 0; k < (int)prop->getKeys().size(); k++) {
            const char *key = prop->getKeys()[k];
            if (k!=0)
                value += ";";
            if (key && key[0])
                value += std::string(key) + "=";
            for (int v = 0; v < prop->getNumValues(key); v++)
                value += std::string(v==0?"":",") + this->quote(prop->getValue(key, v));
        }
        xml.writeAttr("value", value);
        xml.closeTag("property");
    }
}

void Txc2::dumpComponent(XMLWriter& xml, cComponent *component)
{
    const char *tagName = (component==getSimulation()->getSystemModule()) ? "network" : component->isModule() ? "module" : "channel";
    xml.openTag(tagName);
    xml.writeAttr("name", component->getFullName());
    xml.writeAttr("type", component->getNedTypeName());

    if (component->getNumParams() > 0 || component->getProperties()->getNumProperties() > 0) {
        xml.openTag("parameters");
        dumpProperties(xml, component->getProperties());
        for (int i = 0; i < component->getNumParams(); i++) {
            cPar& p = component->par(i);
            xml.openTag("param");
            xml.writeAttr("name", p.getFullName());
            xml.writeAttr("type", replace(cPar::getTypeName(p.getType()), "long", "int"));
            xml.writeAttr("value", p.str());
            dumpProperties(xml, p.getProperties());
            xml.closeTag("param");
        }
        xml.closeTag("parameters");
    }

    if (component->isModule()) {
        cModule *mod = check_and_cast<cModule *>(component);

        if (!cModule::GateIterator(mod).end()) {
            xml.openTag("gates");
            for (cModule::GateIterator i(mod); !i.end(); i++) {
                cGate *gate = i();
                xml.openTag("gate");
                xml.writeAttr("name", gate->getFullName());
                xml.writeAttr("type", cGate::getTypeName(gate->getType()));
                //xml.writeAttr("connected-inside", gate->isConnectedInside());
                //xml.writeAttr("connected-outside", gate->isConnectedOutside());
                this->dumpProperties(xml, gate->getProperties());
                xml.closeTag("gate");
            }
            xml.closeTag("gates");
        }

        if (!cModule::SubmoduleIterator(mod).end()) {
            xml.openTag("submodules");
            for (cModule::SubmoduleIterator submod(mod); !submod.end(); submod++)
                this->dumpComponent(xml, submod());
            xml.closeTag("submodules");
        }

        xml.openTag("connections");
        bool atParent = false;
        for (cModule::SubmoduleIterator it(mod); !atParent; it++) {
            cModule *srcmod = !it.end() ? it() : (atParent=true,mod);
            for (cModule::GateIterator i(srcmod); !i.end(); i++) {
                cGate *srcgate = i();
                cGate *destgate = srcgate->getNextGate();
                if (srcgate->getType()==(atParent ? cGate::INPUT : cGate::OUTPUT) && destgate) {
                    cModule *destmod = destgate->getOwnerModule();
                    xml.openTag("connection");
                    if (srcmod == mod)
                        /* default - leave it out */;
                    else if (srcmod->getParentModule() == mod)
                        xml.writeAttr("src-module", srcmod->getFullName());
                    else
                        xml.writeAttr("src-module", srcmod->getFullPath());
                    xml.writeAttr("src-gate", srcgate->getFullName());
                    if (destmod == mod)
                        /* default - leave it out */;
                    else if (destmod->getParentModule() == mod)
                        xml.writeAttr("dest-module", destmod->getFullName());
                    else
                        xml.writeAttr("dest-module", destmod->getFullPath());
                    xml.writeAttr("dest-gate", destgate->getFullName());
                    cChannel *channel = srcgate->getChannel();
                    if (channel)
                        this->dumpComponent(xml, channel);
                    xml.closeTag("connection");
                }
            }
        }
        xml.closeTag("connections");
    }
    xml.closeTag(tagName);
}
void Txc2::dump(const char *filename)
{
    std::ofstream os(filename, std::ios::out|std::ios::trunc);
    os << "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n";
    XMLWriter xml(os);
    this->dumpComponent(xml, getSimulation()->getSystemModule());
    if (os.fail())
        throw cRuntimeError("Cannot write output file '%s'", filename);
        os.close();
    }
void Txc2::initialize()
{
    this->dump("abc.xml");
    if (strcmp("tic", getName()) == 0) {
        // The `ev' object works like `cout' in C++.
        EV << "Sending initial message\n";
        cMessage *msg = new cMessage("tictocMsg");
        send(msg, "out");
    }
}

void Txc2::handleMessage(cMessage *msg)
{
    // msg->getName() is name of the msg object, here it will be "tictocMsg".
    EV << "Received message `" << msg->getName() << "', sending it out again\n";
    send(msg, "out");
}

