"use strict";

const test = require("node:test");
const assert = require("node:assert/strict");
const E = require("../docs/engine.js");

test("basic strategy: 16 vs 10 hits, 16 vs 6 stands, 16 vs 10 surrenders when allowed", () => {
  assert.equal(E.basicAction(16, 10, false, true, false), "hit");
  assert.equal(E.basicAction(16, 6, false, true, false), "stand");
  assert.equal(E.basicAction(16, 10, false, true, true), "surrender");
  assert.equal(E.basicAction(11, 6, false, true, false), "double");
  assert.equal(E.basicAction(20, 6, false, true, false), "stand");
});

test("hand values treat soft aces", () => {
  const ace = { rank: 1, suit: "♠" };
  const six = { rank: 6, suit: "♥" };
  const king = { rank: 13, suit: "♣" };
  assert.equal(E.handValue([ace, six]).total, 17);
  assert.equal(E.handValue([ace, six]).soft, true);
  assert.equal(E.handValue([ace, six, king]).total, 17);
  assert.equal(E.handValue([ace, six, king]).soft, false);
  assert.equal(E.isBlackjack([ace, king]), true);
});

test("uncounted hole card does not move Hi-Lo until countCard", () => {
  const shoe = E.makeShoe(1, E.mulberry32(7));
  const hole = E.deal(shoe, false);
  assert.equal(shoe.running, 0);
  E.countCard(shoe, hole);
  assert.equal(shoe.running, E.hiLo(hole.rank));
});

test("canSplit only on matched two-card ranks/values", () => {
  const ten = { rank: 10, suit: "♠" };
  const jack = { rank: 11, suit: "♥" };
  const nine = { rank: 9, suit: "♣" };
  assert.equal(E.canSplit([ten, jack]), true);
  assert.equal(E.canSplit([ten, nine]), false);
  assert.equal(E.canSplit([ten]), false);
});

test("betUnits ramps with true count", () => {
  assert.equal(E.betUnits(0), 1);
  assert.ok(E.betUnits(4) > E.betUnits(1));
});

test("indexAction: 16 vs 10 stands at 0, no deviation at -1", () => {
  assert.equal(E.indexAction(16, 10, false, 0, false, false), "stand");
  assert.equal(E.indexAction(16, 10, false, -1, false, false), null);
  assert.equal(E.countAction(16, 10, false, 0, false, false), "stand");
  assert.equal(E.countAction(16, 10, false, -1, false, false), "hit");
});

test("indexAction: 13 vs 2 hits below -1, stands at -1", () => {
  assert.equal(E.indexAction(13, 2, false, -1, false, false), null);
  assert.equal(E.indexAction(13, 2, false, -2, false, false), "hit");
  assert.equal(E.countAction(13, 2, false, -1, false, false), "stand");
  assert.equal(E.countAction(13, 2, false, -2, false, false), "hit");
});

test("countAction falls back to basic when no index applies", () => {
  assert.equal(E.indexAction(20, 6, false, 8, true, true), null);
  assert.equal(E.countAction(20, 6, false, 8, true, true), "stand");
  assert.equal(E.countAction(11, 6, false, 0, true, false), "double");
  assert.equal(E.countAction(16, 10, false, -2, true, true), "surrender");
});

test("INDEX_PLAYS includes 16 vs 10; h17DealerDone matches S17/H17", () => {
  assert.ok(E.INDEX_PLAYS.some((p) => p.name === "16 vs 10"));
  assert.equal(E.h17DealerDone(18, false, false), true);
  assert.equal(E.h17DealerDone(17, false, true), true);
  assert.equal(E.h17DealerDone(17, true, false), true);
  assert.equal(E.h17DealerDone(17, true, true), false);
  assert.equal(E.h17DealerDone(16, true, true), false);
});

test("infinite-deck EV: 20 vs 6 stand beats hit; 16 vs 10 surrender is -0.5", () => {
  const hard20 = E.infiniteDeckEV(20, 6, false, {});
  assert.ok(hard20.stand > hard20.hit);
  const hard16 = E.infiniteDeckEV(16, 10, false, {});
  assert.equal(hard16.surrender, -0.5);
  assert.ok(hard16.stand < 0);
});
