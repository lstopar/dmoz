/**
 * Copyright (c) 2015, Jozef Stefan Institute, Quintelligence d.o.o. and contributors
 * All rights reserved.
 *
 * This source code is licensed under the FreeBSD license found in the
 * LICENSE file in the root directory of this source tree.
 */

const fs = require('fs');

const dmoz = require(__dirname + '/out/dmoz.node');

dmoz.Classifier = class Dmoz {

    constructor(params) {
        let self = this;

        // load binary module
        self._classifier = dmoz;

        // check if classifier file exists
        if (fs.existsSync(params.classifier)) {
            // it does, just load it
            self._classifier.load(params);
        } else {
            // it does not, first create it from complete dmoz
            self._classifier.init(params);
        }

        // load filter
        let filters = fs.readFileSync(params.filter, "utf8").split("\n");
        self.partials = [ ];
        for (let filter of filters) {
            // we only care about paltiar filters
            if (filter.indexOf("*") != -1) {
                // split on prefix and suffix
                let partial = filter.split("*");
                self.partials.push(partial);
            }
        }
        console.log(self.partials.length);
    }

    _cleanCategory(category) {
        let self = this;
        // go over and see if we match a partial
        for (let partial of self.partials) {
            // if we match both prefix and suffix, then we found the match
            if (category.category.startsWith(partial[0]) && category.category.endsWith(partial[1])) {
                return {
                    category: partial[1],
                    weight: category.weight
                }
            }
        }
        // if no match, just remove the "Top/"
        return {
            category: category.category.slice(4),
            weight: category.weight
        };
    }

    classifyTop(text) {
        let self = this;
        // classify document
        let results = self._classifier.classify(text, 1);
        // we just care about top category
        let topCat = results.categories.length > 0 ? results.categories[0] : "Top/";
        // clean the name and return it
        return self._cleanCategory(topCat);
    }

    classify(text, maxCats) {
        let self = this;
        // classify document
        let results = self._classifier.classify(text, 3 * maxCats);
        // clean categories
        results = results.categories.map(cat => self._cleanCategory(cat));
        // get first unique maxCats
        let unique = new Set();
        let result = [];
        for (let category of results) {
            let name = category.category;
            if (!unique.has(name)) {
                unique.add(name);
                result.push(category);
            }
            if (unique.size == maxCats) { break; }
        }
        // return top unique categories
        return result;
    }
}

module.exports = exports = dmoz;
