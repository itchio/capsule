// base functions useful throughout CI scripts

const $ = require("munyx");

$.app_name = function() {
  return "capsule";
};

$.benchmark = true;

module.exports = $;
