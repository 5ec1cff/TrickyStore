package io.github.a13e300.tricky_store.keystore;

import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;
import org.xmlpull.v1.XmlPullParserFactory;

import java.io.IOException;
import java.io.StringReader;
import java.util.HashMap;
import java.util.Map;

public class XMLParser {

    private final String xml;

    public XMLParser(String xml) {
        this.xml = xml;
    }

    public Map<String, String> obtainPath(String path) throws Exception {
        XmlPullParserFactory xmlFactoryObject = XmlPullParserFactory.newInstance();
        XmlPullParser parser = xmlFactoryObject.newPullParser();
        parser.setFeature(XmlPullParser.FEATURE_PROCESS_NAMESPACES, false);
        parser.setInput(new StringReader(xml));

        String[] tags = path.split("\\.");

        return readData(parser, tags, 0, new HashMap<>());
    }

    private Map<String, String> readData(XmlPullParser parser, String[] tags, int index,
                                         Map<String, Integer> tagCounts) throws IOException, XmlPullParserException {
        while (parser.next() != XmlPullParser.END_DOCUMENT) {
            if (parser.getEventType() != XmlPullParser.START_TAG) {
                continue;
            }

            String name = parser.getName();

            if (name.equals(tags[index].split("\\[")[0])) {

                String[] tagParts = tags[index].split("\\[");
                if (tagParts.length > 1) {
                    if (tagCounts.getOrDefault(name, 0) < Integer.parseInt(tagParts[1].replace("]", ""))) {
                        tagCounts.put(name, tagCounts.getOrDefault(name, 0) + 1);
                        return readData(parser, tags, index, tagCounts);
                    } else {
                        if (index == tags.length - 1) {
                            return readAttributes(parser);
                        } else {
                            return readData(parser, tags, index + 1, tagCounts);
                        }
                    }
                } else {
                    if (index == tags.length - 1) {
                        return readAttributes(parser);
                    } else {
                        return readData(parser, tags, index + 1, tagCounts);
                    }
                }
            } else {
                skip(parser);
            }
        }

        throw new XmlPullParserException("Path not found");
    }

    private Map<String, String> readAttributes(XmlPullParser parser) throws IOException, XmlPullParserException {
        Map<String, String> attributes = new HashMap<>();
        for (int i = 0; i < parser.getAttributeCount(); i++) {
            attributes.put(parser.getAttributeName(i), parser.getAttributeValue(i));
        }
        if (parser.next() == XmlPullParser.TEXT) {
            attributes.put("text", parser.getText());
        }
        return attributes;
    }

    private void skip(XmlPullParser parser) throws XmlPullParserException, IOException {
        if (parser.getEventType() != XmlPullParser.START_TAG) {
            throw new IllegalStateException();
        }
        int depth = 1;
        while (depth != 0) {
            switch (parser.next()) {
                case XmlPullParser.END_TAG:
                    depth--;
                    break;
                case XmlPullParser.START_TAG:
                    depth++;
                    break;
            }
        }
    }
}