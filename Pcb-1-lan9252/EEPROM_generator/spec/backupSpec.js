describe("backup", function() {
  
  beforeEach(function() {
  });

  it("should return false for button control", function() {
    // arrange
    const control = { type: 'button' };
    // act
    var result = isBackedUp(control);
    // assert
    expect(result).toEqual(false);
    });
});
